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
    String groq_key;
    String ai_provider; // "gemini" or "groq"

    void begin() {
        // Load from file, fallback to secrets.h
        String jsonStats = fsManager.readFile("/config.json");
        if (jsonStats == "") {
            Serial.println("No config file found, using defaults");
            wifi_ssid = WIFI_SSID;
            wifi_password = WIFI_PASSWORD;
            gemini_key = GEMINI_API_KEY;
            groq_key = ""; 
            ai_provider = "groq"; // Default to Groq as requested
            telegram_token = ""; 
            save(); // Save defaults to file
        } else {
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, jsonStats);
            wifi_ssid = doc["wifi_ssid"].as<String>();
            wifi_password = doc["wifi_password"].as<String>();
            telegram_token = doc["telegram_token"].as<String>();
            gemini_key = doc["gemini_key"].as<String>();
            
            if (doc.containsKey("groq_key")) groq_key = doc["groq_key"].as<String>();
            if (doc.containsKey("ai_provider")) ai_provider = doc["ai_provider"].as<String>();
            else ai_provider = "groq"; // Default fallback
        }
    }

    void save() {
        DynamicJsonDocument doc(1024);
        doc["wifi_ssid"] = wifi_ssid;
        doc["wifi_password"] = wifi_password;
        doc["telegram_token"] = telegram_token;
        doc["gemini_key"] = gemini_key;
        doc["groq_key"] = groq_key;
        doc["ai_provider"] = ai_provider;

        String output;
        serializeJson(doc, output);
        fsManager.writeFile("/config.json", output.c_str());
        Serial.println("Config saved");
    }
};

extern ConfigManager config;

#endif
