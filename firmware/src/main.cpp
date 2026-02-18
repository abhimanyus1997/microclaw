#include <Arduino.h>
#include "secrets.h"
#include "file_system.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "gemini_client.h"
#include "claw_controller.h"
#include "cli.h"
#include "telegram_bot.h"
#include "tools.h"

// Configuration
const int CLAW_SERVO_PIN = 18; 

// Global Objects
FileSystem fsManager;
ConfigManager config;
CLI cli;

// Defer initialization
WifiManager* wifi = nullptr;
GeminiClient* gemini = nullptr;
TelegramBot* bot = nullptr;
ClawController claw(CLAW_SERVO_PIN);
Tools* tools = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Starting MicroClaw ESP32...");

    // Initialize File System
    fsManager.begin();
    
    // Initialize Config
    config.begin();

    // Initialize components
    wifi = new WifiManager(config.wifi_ssid.c_str(), config.wifi_password.c_str());
    gemini = new GeminiClient(config.gemini_key.c_str());
    bot = new TelegramBot(config.telegram_token.c_str()); // Assuming token is in config
    claw.begin();
    tools = new Tools(&claw);
    
    wifi->connect();

    Serial.println("Ready! CLI available. Chat via Telegram.");
}

void loop() {
    // 1. Handle CLI
    cli.checkInput();

    // 2. Poll Telegram (only if connected)
    if (wifi->isConnected()) {
        TelegramBot::Message msg;
        if (bot->getNewMessage(msg)) {
            Serial.print("Telegram User: ");
            Serial.println(msg.text);

            // Notify user we are thinking
            bot->sendMessage(msg.chatId, "Thinking...");

            // Construct Context from Memory
            String memory = fsManager.readFile("/MEMORY.md");
            String contextPrompt = "You are MimiClaw, a physical AI assistant. ";
            if (memory.length() > 0) {
                contextPrompt += "Your memory: " + memory + ". ";
            }
            contextPrompt += "User says: " + msg.text + ". ";
            contextPrompt += "Respond with a JSON object: {\"thought\": \"...\", \"tool\": \"tool_name\", \"args\": { ... }, \"reply\": \"...\"}. ";
            contextPrompt += "Valid tools: 'claw_control' {action: 'OPEN'|'CLOSE'|'WAVE'}, 'memory_write' {content: '...'}, 'memory_read' {}. If no tool needed, set tool to 'none'.";

            // Call Gemini
            String response = gemini->generateContent(contextPrompt);
            Serial.print("Gemini Raw: ");
            Serial.println(response);

            // Parse Response
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, response);

            if (!error) {
                const char* reply = doc["reply"];
                const char* tool = doc["tool"];
                
                String toolResult = "";
                if (tool && String(tool) != "none") {
                     toolResult = tools->execute(String(tool), doc["args"]);
                     Serial.println("Tool Result: " + toolResult);
                }

                String finalReply = String(reply);
                if (toolResult != "") {
                    // Start a new turn if tool produced output? 
                    // For simplicity, just append result or rely on 'reply' being accurate.
                    // Ideally, we'd feed the tool result back to LLM.
                    // For now, let's just send the reply.
                    finalReply += "\n[Tool: " + toolResult + "]";
                }
                
                bot->sendMessage(msg.chatId, finalReply);
            } else {
                bot->sendMessage(msg.chatId, "Error parsing my own thought... " + response);
            }
        }
    } else {
        // Reconnect logic inside wifi manager or here? 
        // WifiManager handles connection in begin(), simple check here.
        // For production, add auto-reconnect logic.
    }
    
    delay(100); // Prevent tight loop
}
