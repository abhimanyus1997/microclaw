#ifndef CLI_H
#define CLI_H

#include "config_manager.h"

class CLI {
public:
    void checkInput() {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            handleCommand(input);
        }
    }

private:
    void handleCommand(String input) {
        if (input.startsWith("wifi_set ")) {
            int spaceIndex = input.indexOf(' ', 9);
            if (spaceIndex > 0) {
                config.wifi_ssid = input.substring(9, spaceIndex);
                config.wifi_password = input.substring(spaceIndex + 1);
                config.save();
                Serial.println("WiFi Configured. Rebooting...");
                delay(1000);
                ESP.restart();
            } else {
                Serial.println("Usage: wifi_set <ssid> <password>");
            }
        } else if (input.startsWith("set_tg_token ")) {
            config.telegram_token = input.substring(13);
            config.save();
            Serial.println("Telegram Token Saved.");
        } else if (input.startsWith("set_api_key ")) {
            config.gemini_key = input.substring(12);
            config.save();
            Serial.println("API Key Saved.");
        } else if (input == "config_show") {
            Serial.println("--- Config ---");
            Serial.print("SSID: "); Serial.println(config.wifi_ssid);
            Serial.print("Telegram: "); Serial.println(config.telegram_token.substring(0, 5) + "...");
            Serial.print("API Key: "); Serial.println(config.gemini_key.substring(0, 5) + "...");
        } else if (input == "restart") {
            ESP.restart();
        } else {
            Serial.println("Unknown command. Available: wifi_set, set_tg_token, set_api_key, config_show, restart");
        }
    }
};

#endif
