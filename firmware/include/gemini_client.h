#ifndef GEMINI_CLIENT_H
#define GEMINI_CLIENT_H

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class GeminiClient {
public:
    GeminiClient(const char* apiKey) : _apiKey(apiKey) {}

    String generateContent(String prompt) {
        if (WiFi.status() != WL_CONNECTED) {
            return "{\"error\": \"WiFi not connected\"}";
        }

        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure(); // For prototyping; use root CA for production

        String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + String(_apiKey);

        if (!http.begin(client, url)) {
            return "{\"error\": \"Unable to connect\"}";
        }

        http.addHeader("Content-Type", "application/json");

        // Construct JSON payload using ArduinoJson
        // Use DynamicJsonDocument. Size calculation: prompt length + overhead. 
        // 4KB is usually enough for simple prompts.
        DynamicJsonDocument doc(4096);
        JsonObject content = doc.createNestedObject("contents");
        JsonArray parts = content.createNestedArray("parts");
        JsonObject part = parts.createNestedObject();
        part["text"] = prompt;

        String payload;
        serializeJson(doc, payload);

        int httpCode = http.POST(payload);
        String result = "";

        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            
            // Parse response to find candidates[0].content.parts[0].text
            DynamicJsonDocument responseDoc(8192); // Larger buffer for response
            DeserializationError error = deserializeJson(responseDoc, response);

            if (!error) {
                const char* outputText = responseDoc["candidates"][0]["content"]["parts"][0]["text"];
                if (outputText) {
                    result = String(outputText);
                } else {
                    result = "{\"error\": \"No text in response\"}";
                }
            } else {
                result = "{\"error\": \"JSON parsing failed\"}";
            }
        } else {
            result = "{\"error\": \"HTTP Error " + String(httpCode) + "\"}";
        }

        http.end();
        return result;
    }

private:
    const char* _apiKey;
};

#endif
