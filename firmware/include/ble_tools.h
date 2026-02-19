#ifndef BLE_TOOLS_H
#define BLE_TOOLS_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>

class BleTools {
public:
    static BLEClient* pClient;

    static String scan() {
        Serial.println("BLE: Scanning for 5 seconds...");
        
        BLEDevice::init("MicroClaw-ESP32");
        
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setActiveScan(true); 
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        
        BLEScanResults foundDevices = pBLEScan->start(5, false);
        int count = foundDevices.getCount();
        Serial.printf("BLE: Found %d devices\n", count);
        
        if (count == 0) {
            pBLEScan->clearResults();
            BLEDevice::deinit(true); // Release memory
            return "No devices found.";
        }

        // Reduced buffer size to prevent heap exhaustion
        DynamicJsonDocument doc(2048);
        JsonArray array = doc.to<JsonArray>();
        
        // LIMIT to top 10 devices to save RAM
        int max_devices = (count > 10) ? 10 : count;
        
        for (int i = 0; i < max_devices; i++) {
            BLEAdvertisedDevice device = foundDevices.getDevice(i);
            JsonObject obj = array.createNestedObject();
            
            std::string name = device.getName();
            String safeName = "";
            
            // SANITIZE NAME: Remove control characters that break JSON
            if (name.length() > 0) {
                for (char c : name) {
                    if (isPrintable(c) && c != '"' && c != '\\') {
                        safeName += c;
                    }
                }
            }
            if (safeName.length() == 0) safeName = "Unknown";
            
            obj["name"] = safeName;
            obj["addr"] = device.getAddress().toString().c_str();
            obj["rssi"] = device.getRSSI();
            
            if (device.haveServiceUUID()) {
                 obj["service"] = device.getServiceUUID().toString().c_str();
            }
        }
        
        String output;
        serializeJson(doc, output);
        
        pBLEScan->clearResults(); 
        BLEDevice::deinit(true); // Important to free BT stack memory for SSL
        return output;
    }

    static String connect(String address) {
        if (pClient && pClient->isConnected()) {
            return "Already connected. Disconnect first.";
        }

        BLEDevice::init("MicroClaw-ESP32");
        if (!pClient) {
            pClient = BLEDevice::createClient();
        }

        Serial.print("Connecting to: ");
        Serial.println(address);

        if (pClient->connect(BLEAddress(address.c_str()))) {
            Serial.println("Connected to server");
            
            // List services
            String services = "Connected. Services: ";
            std::map<std::string, BLERemoteService*>* pServices = pClient->getServices();
            if (pServices) {
                for (auto const& entry : *pServices) {
                    services += String(entry.first.c_str()) + ", ";
                }
            }
            return services;
        } else {
            return "Failed to connect.";
        }
    }

    static String disconnect() {
        if (pClient && pClient->isConnected()) {
            pClient->disconnect();
            BLEDevice::deinit(true); // Free memory
            return "Disconnected.";
        }
        return "Not connected.";
    }
};

BLEClient* BleTools::pClient = nullptr;

#endif
