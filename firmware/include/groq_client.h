#ifndef GROQ_CLIENT_H
#define GROQ_CLIENT_H

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "common.h"

class GroqClient {
public:
    GroqClient(const char* apiKey) : _apiKey(apiKey) {}

    String generateContent(String prompt) {
        if (WiFi.status() != WL_CONNECTED) {
            return "{\"error\": \"WiFi not connected\"}";
        }

        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure(); // For prototyping

        String url = "https://api.groq.com/openai/v1/chat/completions";

        if (!http.begin(client, url)) {
            return "{\"error\": \"Unable to connect to Groq\"}";
        }

        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", "Bearer " + String(_apiKey));

        // Construct JSON payload using ArduinoJson
        DynamicJsonDocument doc(4096);
        doc["model"] = "openai/gpt-oss-120b"; // Default model as requested, or can make configurable
        doc["temperature"] = 1;
        doc["max_completion_tokens"] = 1024; // Adjusted for ESP32 memory constraints vs 8192
        doc["top_p"] = 1;
        doc["stream"] = false; // We use non-streaming for simplicity in this firmware version

        JsonArray messages = doc.createNestedArray("messages");
        JsonObject userMsg = messages.createNestedObject();
        userMsg["role"] = "user";
        userMsg["content"] = prompt;

        String payload;
        serializeJson(doc, payload);

        int httpCode = http.POST(payload);
        String result = "";

        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            
            // Parse response
            DynamicJsonDocument responseDoc(8192);
            DeserializationError error = deserializeJson(responseDoc, response);

            if (!error) {
                // Check for tool calls first
                JsonArray toolCalls = responseDoc["choices"][0]["message"]["tool_calls"];
                if (toolCalls.size() > 0) {
                    // Assuming only one tool call for simplicity in this example
                    JsonObject toolCall = toolCalls[0];
                    String functionName = toolCall["function"]["name"];
                    JsonObject funcArgs = toolCall["function"]["arguments"];
                    String toolOutput = "";

                    // Placeholder for actual tool execution.
                    // You would need to define 'claw' and 'SystemTools' objects/functions
                    // in your main sketch or pass them to GroqClient.
                    // For this example, we'll assume they are accessible.
                    // Example: extern Claw claw; extern SystemTools systemTools;

                    if (functionName == "get_system_stats" || functionName == "claw_control" || functionName == "gpio_control") {
                         // Translate Native Tool Call to the JSON format main.cpp expects
                        DynamicJsonDocument jsonDoc(2048);
                        jsonDoc["thought"] = "Agent invoked native tool: " + functionName;
                        jsonDoc["tool"] = functionName;
                        jsonDoc["args"] = funcArgs; 
                        jsonDoc["reply"] = "Executing " + functionName + "...";
                        
                        serializeJson(jsonDoc, result);
                    } else {
                         result = "{\"thought\": \"Unknown tool called\", \"tool\": \"none\", \"reply\": \"Error: Model tried to call unknown tool " + functionName + "\"}";
                    }

                } else {
                    // Groq/OpenAI format: choices[0].message.content
                    const char* outputText = responseDoc["choices"][0]["message"]["content"];
                    if (outputText) {
                        result = String(outputText);
                    } else {
                        result = "{\"error\": \"No text in Groq response\"}";
                    }
                }
            } else {
                result = "{\"error\": \"JSON parsing failed\"}";
            }
        } else {
            String errorPayload = http.getString();
            result = "{\"error\": \"HTTP Error " + String(httpCode) + ": " + errorPayload + "\"}";
        }

        http.end();
        return result;
    }

private:
    const char* _apiKey;
};

#endif
