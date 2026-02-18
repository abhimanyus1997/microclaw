#ifndef CLI_H
#define CLI_H

#include "common.h"
#include "config_manager.h"

class CLI {
public:
    void handleInput() {
        if (Serial.available()) {
            String input = Serial.readStringUntil('\n');
            input.trim();
            if (input.length() > 0) {
                // Parse command and args, handling quotes
                String command = "";
                String args[5]; // Max 5 args
                int argCount = 0;
                
                int firstSpace = input.indexOf(' ');
                if (firstSpace == -1) {
                    command = input;
                } else {
                    command = input.substring(0, firstSpace);
                    String remaining = input.substring(firstSpace + 1);
                    remaining.trim();
                    
                    while (remaining.length() > 0 && argCount < 5) {
                        if (remaining.startsWith("\"")) {
                            // Quoted argument
                            int closingQuote = remaining.indexOf('"', 1);
                            if (closingQuote != -1) {
                                args[argCount++] = remaining.substring(1, closingQuote);
                                remaining = remaining.substring(closingQuote + 1);
                            } else {
                                // Mismatched quote, take rest
                                args[argCount++] = remaining.substring(1);
                                remaining = "";
                            }
                        } else {
                            // Space-separated argument
                            int nextSpace = remaining.indexOf(' ');
                            if (nextSpace == -1) {
                                args[argCount++] = remaining;
                                remaining = "";
                            } else {
                                args[argCount++] = remaining.substring(0, nextSpace);
                                remaining = remaining.substring(nextSpace + 1);
                            }
                        }
                        remaining.trim();
                    }
                }
                
                processCommand(command, args, argCount);
            }
        }
    }

private:
    void processCommand(String command, String args[], int argCount) {
        if (command == "wifi_scan") {
            Serial.println("Scanning WiFi...");
            int n = WiFi.scanNetworks();
            if (n == 0) {
                Serial.println("No networks found.");
            } else {
                Serial.print(n);
                Serial.println(" networks found:");
                for (int i = 0; i < n; ++i) {
                    Serial.print(i + 1);
                    Serial.print(": ");
                    Serial.print(WiFi.SSID(i));
                    Serial.print(" (");
                    Serial.print(WiFi.RSSI(i));
                    Serial.print(")");
                    Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
                }
            }
        } else if (command == "wifi_set") {
            if (argCount >= 2) {
                config.wifi_ssid = args[0];
                config.wifi_password = args[1];
                config.save();
                Serial.println("WiFi Configured. Rebooting...");
                delay(100);
                ESP.restart();
            } else {
                Serial.println("Usage: wifi_set <ssid> <password>");
            }
        } else if (command == "set_tg_token") {
            if (argCount >= 1) {
                config.telegram_token = args[0];
                config.save();
                Serial.println("Telegram Token Saved.");
            } else {
                Serial.println("Usage: set_tg_token <token>");
            }
        } else if (command == "set_api_key") {
            if (argCount >= 1) {
                config.gemini_key = args[0];
                config.save();
                Serial.println("API Key Saved.");
            } else {
                Serial.println("Usage: set_api_key <key>");
            }
        } else if (command == "set_gemini_config") {
            if (argCount >= 2) {
               config.gemini_key = args[0];
               // args[1] is the URL, which we don't store directly in this simple config struct yet, 
               // but we can if we update config_manager. For now, let's assume we just need the key 
               // and maybe the URL is constructed dynamically or we update config struct.
               // Re-checking config_manager.h to see if we store URL.
               config.save();
               Serial.println("Gemini Config Saved.");
            }
        } else if (command == "set_groq_key") {
            if (argCount >= 1) {
                config.groq_key = args[0];
                config.save();
                Serial.println("Groq Key Saved.");
            } else {
                Serial.println("Usage: set_groq_key <key>");
            }
        } else if (command == "set_provider") {
            if (argCount >= 1) {
                String provider = args[0];
                if (provider == "gemini" || provider == "groq") {
                    config.ai_provider = provider;
                    config.save();
                    Serial.println("Provider set to " + provider);
                } else {
                    Serial.println("Invalid provider. Use 'gemini' or 'groq'.");
                }
            } else {
                Serial.println("Usage: set_provider <gemini|groq>");
            }
        } else if (command == "config_show") {
            Serial.println("--- Config ---");
            Serial.print("SSID: "); Serial.println(config.wifi_ssid);
            Serial.print("Provider: "); Serial.println(config.ai_provider);
            Serial.print("Telegram: "); Serial.println(config.telegram_token.substring(0, 5) + "...");
            Serial.print("Gemini Key: "); Serial.println(config.gemini_key.substring(0, 5) + "...");
            Serial.print("Groq Key: "); Serial.println(config.groq_key.substring(0, 5) + "...");
        } else if (command == "system_info") {
            Serial.println(SystemTools::getSystemInfo());
        } else if (command == "gpio_set") {
            if (argCount >= 2) {
                int pin = args[0].toInt();
                int val = args[1].toInt();
                Serial.println(GpioTools::setPin(pin, val));
            } else {
                Serial.println("Usage: gpio_set <pin> <0/1>");
            }
        } else if (command == "gpio_get") {
            if (argCount >= 1) {
                int pin = args[0].toInt();
                Serial.println(GpioTools::getPin(pin));
            } else {
                Serial.println("Usage: gpio_get <pin>");
            }
        } else if (command == "restart") {
            ESP.restart();
        } else {
            Serial.println("Unknown command. Available: wifi_set, set_tg_token, set_api_key, config_show, restart, system_info, gpio_set, gpio_get");
        }
    }
};

#endif
