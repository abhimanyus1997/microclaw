#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>

class BleTools {
public:
    static String scan() {
        Serial.println("BLE: Scanning for 5 seconds...");
        
        // Initialize BLE if not already (safe to call multiple times? usually yes, or check logic)
        // BLEDevice::init is robust given we are in a single-threaded loop mostly.
        // But better to check. We'll just init.
        BLEDevice::init("MicroClaw-ESP32");
        
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setActiveScan(true); // Active scan for names
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        
        // Scan for 5 seconds
        BLEScanResults foundDevices = pBLEScan->start(5, false);
        int count = foundDevices.getCount();
        Serial.printf("BLE: Found %d devices\n", count);
        
        if (count == 0) {
            pBLEScan->clearResults();
            return "No devices found.";
        }

        DynamicJsonDocument doc(4096);
        JsonArray array = doc.to<JsonArray>();
        
        for (int i = 0; i < count; i++) {
            BLEAdvertisedDevice device = foundDevices.getDevice(i);
            JsonObject obj = array.createNestedObject();
            
            // Name (handle empty)
            std::string name = device.getName();
            if (name.length() > 0) {
                obj["name"] = name.c_str();
            } else {
                obj["name"] = "Unknown";
            }
            
            obj["addr"] = device.getAddress().toString().c_str();
            obj["rssi"] = device.getRSSI();
            
            if (device.haveServiceUUID()) {
                 obj["service"] = device.getServiceUUID().toString().c_str();
            }
        }
        
        String output;
        serializeJson(doc, output);
        
        pBLEScan->clearResults(); // Release memory
        return output;
    }
};

#endif
