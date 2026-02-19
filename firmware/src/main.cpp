#include "common.h"
#include "secrets.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "gemini_client.h"
#include "groq_client.h" // Added Groq
#include "cli.h"
#include "telegram_bot.h"
#include "tools.h"
#include "web_server.h"

/*
 * MicroClaw Firmware
 * Author: Abhimanyu Singh
 * Email: abhimanyus1997@gmail.com
 */

// Global Objects
FileSystem fsManager;
ConfigManager config;
CLI cli;

// Defer initialization
WifiManager* wifi = nullptr;
GeminiClient* gemini = nullptr;
GroqClient* groq = nullptr;
TelegramBot* bot = nullptr;
Tools* tools = nullptr;
WebInterface* webServer = nullptr;

// Unified Agent Logic
String handleAgentRequest(String userText, JsonArray history = JsonArray(), int depth = 0) {
    if (depth > 5) return "{\"reply\":\"Too much recursion!\"}";
    
    Serial.print("User (D");
    Serial.print(depth);
    Serial.print("): ");
    Serial.println(userText);

    // Construct Context from Memory
    String memory = fsManager.readFile("/MEMORY.md");
    String contextPrompt = "You are MicroClaw, a physical AI assistant running on an ESP32, created by Abhimanyu Singh. ";
    contextPrompt += "You can interact with hardware via GPIOs, scan WiFi, and manage system stats. ";
    if (memory.length() > 0) {
        contextPrompt += "Your memory (long-term): " + memory + ". ";
    }
    
    if (!history.isNull() && history.size() > 0) {
        contextPrompt += "Recent conversation history (short-term): ";
        for (JsonVariant m : history) {
            String s = m["sender"].as<String>();
            String t = m["text"].as<String>();
            String tool_res = m["tool_result"].as<String>();
            
            contextPrompt += (s == "user" ? "User: " : "AI: ") + t;
            if (tool_res.length() > 0 && tool_res != "null") {
                contextPrompt += " [Tool Result: " + tool_res + "]";
            }
            contextPrompt += " | ";
        }
    }
    
    if (depth > 0) {
        contextPrompt += "SYSTEM: The tool you called returned: " + userText + ". ";
        contextPrompt += "Based on this hardware data, provide your final friendly reply to the user. Set tool to 'none'.";
    } else {
        contextPrompt += "Current User message: " + userText + ". ";
    }

    contextPrompt += "Respond with a JSON object: {\"thought\": \"...\", \"tool\": \"tool_name\", \"args\": { ... }, \"reply\": \"...\"}. ";
    contextPrompt += "Valid tools: 'get_system_stats' {}, 'wifi_scan' {}, 'ble_scan' {}, 'ble_connect' {address: '...'}, 'ble_disconnect' {}, 'memory_write' {content: '...'}, 'memory_read' {}. ";
    contextPrompt += "'run_script' { script: [ {cmd: \"gpio\", pin: 2, state: 1}, {cmd: \"delay\", ms: 1000}, {cmd: \"loop\", count: 5, steps: [...]} ] }. ";
    contextPrompt += "Use 'run_script' for ALL hardware control (blinking, patterns, resizing). ";
    contextPrompt += "IMPORTANT: 'run_script' is NON-BLOCKING. The script runs in the background. ";
    contextPrompt += "Your reply should be: 'I have started the script...' instead of 'I executed...'. The user will see the action happen immediately after your reply.";

    // Call AI Provider
    String response;
    if (config.ai_provider == "groq" && groq) {
        Serial.println("Using Groq...");
        response = groq->generateContent(contextPrompt);
    } else {
        Serial.println("Using Gemini...");
        response = gemini->generateContent(contextPrompt);
    }

    Serial.print("AI Raw: ");
    Serial.println(response);

    // Parse Response
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
        // Check for API Error
        if (doc.containsKey("error")) {
            String errorMsg = doc["error"].as<String>();
            Serial.println("AI Error: " + errorMsg);
            return "{\"reply\":\"I'm having trouble thinking right now. (" + errorMsg + ")\"}";
        }

        const char* reply = doc["reply"];
        const char* thought = doc["thought"];
        const char* tool = doc["tool"];
        
        String toolResult = "";
        if (tool && String(tool) != "none" && depth == 0) {
            toolResult = tools->execute(String(tool), doc["args"]);
            Serial.println("Tool Result: " + toolResult);
            
            // SECOND CALL (Follow-up)
            String secondResponse = handleAgentRequest(toolResult, history, depth + 1);
            
            // Extract the final reply from the second call
            DynamicJsonDocument secondDoc(4096);
            DeserializationError err = deserializeJson(secondDoc, secondResponse);
            
            const char* finalTxt = "I executed the tool but had trouble summarizing the result.";
            if (!err && secondDoc.containsKey("reply")) {
                finalTxt = secondDoc["reply"];
            }

            DynamicJsonDocument outDoc(4096);
            outDoc["reply"] = finalTxt;
            outDoc["thought"] = thought; // Keep original thought
            outDoc["tool"] = tool;
            outDoc["tool_result"] = toolResult;
            
            String output;
            serializeJson(outDoc, output);
            return output;
        }

        // Return structured JSON for the Web UI (Base case or Depth > 0)
        DynamicJsonDocument outDoc(4096);
        outDoc["reply"] = reply;
        outDoc["thought"] = thought;
        outDoc["tool"] = (tool && String(tool) != "none") ? tool : "";
        outDoc["tool_result"] = toolResult;
        
        String output;
        serializeJson(outDoc, output);
        return output;
    } else {
        return "{\"reply\":\"Error parsing my own thought: " + response + "\"}";
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n--- MicroClaw Firmware Starting ---");
    Serial.println("Author: Abhimanyu Singh (abhimanyus1997@gmail.com)");
    
    // Initialize File System (auto-format if failed)
    fsManager.begin();
    if (!LittleFS.exists("/MEMORY.md")) {
        fsManager.writeFile("/MEMORY.md", "MicroClaw Memory initialized.\n");
    }
    
    // Initialize Config
    config.begin();
    // config.load(); // Loaded in begin()
    
    Serial.println("Starting MicroClaw ESP32...");

    // Initialize components
    wifi = new WifiManager(config.wifi_ssid.c_str(), config.wifi_password.c_str(), DEVICE_HOSTNAME);
    gemini = new GeminiClient(config.gemini_key.c_str());
    groq = new GroqClient(config.groq_key.c_str());
    
    // Optional Telegram
    if (config.telegram_token.length() > 0) {
        bot = new TelegramBot(config.telegram_token.c_str());
        Serial.println("Telegram Bot Enabled");
    } else {
        Serial.println("Telegram Bot Disabled (No Token)");
    }

    tools = new Tools();
    
    // Initialize Web Server
    webServer = new WebInterface();
    
    // Connect to WiFi First (Important for TCP Stack)
    wifi->connect();

    // Bind agent logic to web server & Start
    webServer->begin([](String body) -> String {
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, body);
        String text = doc["text"].as<String>();
        JsonArray history = doc["history"].as<JsonArray>();
        return handleAgentRequest(text, history);
    });

    Serial.println("Ready! CLI available.");
    if (config.telegram_token.length() > 0) Serial.println("Chat via Telegram.");
    
    if (wifi->isConnected()) {
        Serial.print("Chat via Web: http://"); Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi Not Connected. Run 'microclaw.py setup' to configure.");
    }
}

void loop() {
    // 1. Handle CLI
    cli.handleInput();

    // 2. Handle Web Server
    if (webServer) webServer->handleClient();

    // 3. Poll Telegram (only if enabled & connected)
    if (bot && wifi->isConnected()) {
        TelegramBot::Message msg;
        if (bot->getNewMessage(msg)) {
            // Notify user we are thinking
            bot->sendMessage(msg.chatId, "Thinking...");
            
            // Process Request
            String reply = handleAgentRequest(msg.text);
            
            // Send Reply
            bot->sendMessage(msg.chatId, reply);
        }
    }
    
    delay(50); 
}
