#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

class WifiManager {
public:
    WifiManager(const char* ssid, const char* password) : _ssid(ssid), _password(password) {}

    void connect() {
        Serial.print("Connecting to WiFi: ");
        Serial.println(_ssid);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(_ssid, _password);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected.");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\nWiFi connection failed.");
        }
    }

    bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }

private:
    const char* _ssid;
    const char* _password;
};

#endif
