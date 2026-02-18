#ifndef WIFI_TOOLS_H
#define WIFI_TOOLS_H

#include <WiFi.h>
#include <ArduinoJson.h>

class WifiTools {
public:
    static String scan() {
        Serial.println("WiFi: Scanning...");
        int n = WiFi.scanNetworks();
        
        StaticJsonDocument<1024> doc;
        JsonArray networks = doc.to<JsonArray>();
        
        if (n == 0) {
            return "No networks found";
        } else {
            // Only return top 5 strongest networks to save token space
            int limit = (n > 5) ? 5 : n;
            for (int i = 0; i < limit; ++i) {
                JsonObject net = networks.createNestedObject();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
                net["enc"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
            }
        }
        
        String jsonString;
        serializeJson(doc, jsonString);
        WiFi.scanDelete();
        return jsonString;
    }
};

#endif
