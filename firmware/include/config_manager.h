#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ArduinoJson.h>
#include "file_system.h"
#include "secrets.h" // Fallback defaults

class ConfigManager {
public:
    String wifi_ssid;
    String wifi_password;
    String telegram_token;
    String gemini_key;

    void begin() {
        // Load from file, fallback to secrets.h
        String jsonStats = fsManager.readFile("/config.json");
        if (jsonStats == "") {
            Serial.println("No config file found, using defaults");
            wifi_ssid = WIFI_SSID;
            wifi_password = WIFI_PASSWORD;
            gemini_key = GEMINI_API_KEY;
            telegram_token = ""; // No default for telegram in secrets.h yet
            save(); // Save defaults to file
        } else {
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, jsonStats);
            wifi_ssid = doc["wifi_ssid"].as<String>();
            wifi_password = doc["wifi_password"].as<String>();
            telegram_token = doc["telegram_token"].as<String>();
            gemini_key = doc["gemini_key"].as<String>();
        }
    }

    void save() {
        DynamicJsonDocument doc(1024);
        doc["wifi_ssid"] = wifi_ssid;
        doc["wifi_password"] = wifi_password;
        doc["telegram_token"] = telegram_token;
        doc["gemini_key"] = gemini_key;

        String output;
        serializeJson(doc, output);
        fsManager.writeFile("/config.json", output.c_str());
        Serial.println("Config saved");
    }
};

extern ConfigManager config;

#endif
