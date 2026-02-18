#ifndef GEMINI_CLIENT_H
#define GEMINI_CLIENT_H

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "common.h"

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

        // Construct JSON payload with Tools
        DynamicJsonDocument doc(4096);
        
        // 1. Define Tools
        JsonArray toolsArray = doc.createNestedArray("tools");
        JsonObject funcDeclObj = toolsArray.createNestedObject();
        JsonArray funcDecls = funcDeclObj.createNestedArray("function_declarations");

        // Tool: get_system_stats
        JsonObject t1 = funcDecls.createNestedObject();
        t1["name"] = "get_system_stats";
        t1["description"] = "Get current system statistics like heap memory, uptime, cpu frequency, and flash size.";

        // Tool: claw_control
        JsonObject t2 = funcDecls.createNestedObject();
        t2["name"] = "claw_control";
        t2["description"] = "Control the claw mechanism.";
        JsonObject t2Params = t2.createNestedObject("parameters");
        t2Params["type"] = "OBJECT";
        JsonObject t2Props = t2Params.createNestedObject("properties");
        JsonObject t2P1 = t2Props.createNestedObject("action");
        t2P1["type"] = "STRING";
        t2P1["description"] = "Action to perform: 'open' or 'close'";
        JsonArray t2Req = t2Params.createNestedArray("required");
        t2Req.add("action");

         // Tool: gpio_control
        JsonObject t3 = funcDecls.createNestedObject();
        t3["name"] = "gpio_control";
        t3["description"] = "Control GPIO pins on the ESP32.";
        JsonObject t3Params = t3.createNestedObject("parameters");
        t3Params["type"] = "OBJECT";
        JsonObject t3Props = t3Params.createNestedObject("properties");
        
        JsonObject t3P1 = t3Props.createNestedObject("pin");
        t3P1["type"] = "INTEGER";
        t3P1["description"] = "GPIO Pin Number";
        
        JsonObject t3P2 = t3Props.createNestedObject("mode");
        t3P2["type"] = "STRING";
        t3P2["description"] = "Mode: 'output' or 'input'";

        JsonObject t3P3 = t3Props.createNestedObject("state");
        t3P3["type"] = "INTEGER";
        t3P3["description"] = "State for output (0/1). Ignored for input.";

        JsonArray t3Req = t3Params.createNestedArray("required");
        t3Req.add("pin");
        t3Req.add("mode");


        // 2. User Content
        JsonObject content = doc.createNestedObject("contents");
        content["role"] = "user";
        JsonArray parts = content.createNestedArray("parts");
        JsonObject part = parts.createNestedObject();
        part["text"] = prompt;

        String payload;
        serializeJson(doc, payload);

        int httpCode = http.POST(payload);
        String result = "";

        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            
            DynamicJsonDocument responseDoc(8192);
            DeserializationError error = deserializeJson(responseDoc, response);

            if (!error) {
                // Check if model wants to call a function
                // Gemini Format: candidates[0].content.parts[0].functionCall
                JsonObject funcCall = responseDoc["candidates"][0]["content"]["parts"][0]["functionCall"];
                
                if (!funcCall.isNull()) {
                    String funcName = funcCall["name"].as<String>();
                    JsonObject args = funcCall["args"];
                    String toolOutput = "";

                    if (funcName == "get_system_stats" || funcName == "claw_control" || funcName == "gpio_control" || funcName == "memory_write" || funcName == "memory_read") {
                        // Translate Native Tool Call to the JSON format main.cpp expects
                        // main.cpp expects: {"thought": "...", "tool": "name", "args": {...}, "reply": "..."}
                        
                        DynamicJsonDocument jsonDoc(2048);
                        jsonDoc["thought"] = "Agent invoked native tool: " + funcName;
                        jsonDoc["tool"] = funcName;
                        jsonDoc["args"] = args; // Copy args object directly
                        jsonDoc["reply"] = "Executing " + funcName + "...";
                        
                        serializeJson(jsonDoc, result);
                    } else {
                        // Unknown tool
                         result = "{\"thought\": \"Unknown tool called\", \"tool\": \"none\", \"reply\": \"Error: Model tried to call unknown tool " + funcName + "\"}";
                    }

                } else {
                    const char* outputText = responseDoc["candidates"][0]["content"]["parts"][0]["text"];
                    if (outputText) {
                        result = String(outputText);
                    } else {
                        result = "{\"error\": \"No text in response\"}";
                    }
                }
            } else {
                result = "{\"error\": \"JSON parsing failed\"}";
            }
        } else {
             // Debug info
             String err = http.getString();
            result = "{\"error\": \"HTTP Error " + String(httpCode) + ": " + err + "\"}";
        }

        http.end();
        return result;
    }

private:
    const char* _apiKey;
};

#endif
