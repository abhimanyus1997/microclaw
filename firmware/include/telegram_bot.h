#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

class TelegramBot {
public:
    TelegramBot(const char* token) : _token(token) {}

    struct Message {
        String text;
        String chatId;
        long updateId;
    };

    // Poll for new messages. Returns true if a new message is found.
    bool getNewMessage(Message &msg) {
        if (_token == "") return false;

        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;

        String url = "https://api.telegram.org/bot" + String(_token) + "/getUpdates?offset=" + String(_lastUpdateId + 1) + "&limit=1&timeout=0";
        
        if (http.begin(client, url)) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                DynamicJsonDocument doc(4096);
                deserializeJson(doc, payload);

                JsonArray result = doc["result"];
                if (result.size() > 0) {
                    JsonObject update = result[0];
                    _lastUpdateId = update["update_id"];
                    msg.updateId = _lastUpdateId;
                    msg.chatId = update["message"]["chat"]["id"].as<String>();
                    msg.text = update["message"]["text"].as<String>();
                    
                    http.end();
                    return true;
                }
            } else {
               // Serial.println("Telegram Poll Failed: " + String(httpCode));
            }
            http.end();
        }
        return false;
    }

    void sendMessage(String chatId, String text) {
        if (_token == "") return;

        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;

        String url = "https://api.telegram.org/bot" + String(_token) + "/sendMessage";
        
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            
            DynamicJsonDocument doc(2048);
            doc["chat_id"] = chatId;
            doc["text"] = text;
            String payload;
            serializeJson(doc, payload); // Handles escaping

            int httpCode = http.POST(payload);
            if (httpCode != HTTP_CODE_OK) {
                Serial.println("Telegram Send Failed: " + http.getString());
            }
            http.end();
        }
    }

private:
    const char* _token;
    long _lastUpdateId = 0;
};

#endif
