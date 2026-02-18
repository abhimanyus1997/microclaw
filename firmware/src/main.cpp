#include <Arduino.h>
#include "secrets.h"
#include "file_system.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "gemini_client.h"
#include "groq_client.h" // Added Groq
#include "claw_controller.h"
#include "cli.h"
#include "telegram_bot.h"
#include "tools.h"
#include "web_server.h"

/*
 * MicroClaw Firmware
 * Author: Abhimanyu Singh
 * Email: abhimanyus1997@gmail.com
 */

// Configuration
const int CLAW_SERVO_PIN = 18; 
const char* DEVICE_HOSTNAME = "microclaw";

// Global Objects
FileSystem fsManager;
ConfigManager config;
CLI cli;

// Defer initialization
WifiManager* wifi = nullptr;
GeminiClient* gemini = nullptr;
GroqClient* groq = nullptr;
TelegramBot* bot = nullptr;
ClawController claw(CLAW_SERVO_PIN);
Tools* tools = nullptr;
WebInterface* webServer = nullptr;

// Unified Agent Logic
String handleAgentRequest(String userText) {
    Serial.print("User: ");
    Serial.println(userText);

    // Construct Context from Memory
    String memory = fsManager.readFile("/MEMORY.md");
    String contextPrompt = "You are MicroClaw, a physical AI assistant created by Abhimanyu Singh. ";
    if (memory.length() > 0) {
        contextPrompt += "Your memory: " + memory + ". ";
    }
    contextPrompt += "User says: " + userText + ". ";
    contextPrompt += "Respond with a JSON object: {\"thought\": \"...\", \"tool\": \"tool_name\", \"args\": { ... }, \"reply\": \"...\"}. ";
    contextPrompt += "Valid tools: 'claw_control' {action: 'OPEN'|'CLOSE'|'WAVE'}, 'memory_write' {content: '...'}, 'memory_read' {}. If no tool needed, set tool to 'none'.";

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
            return "I'm having trouble thinking right now. (" + errorMsg + ")";
        }

        const char* reply = doc["reply"];
        const char* tool = doc["tool"];
        
        String toolResult = "";
        if (tool && String(tool) != "none") {
                toolResult = tools->execute(String(tool), doc["args"]);
                Serial.println("Tool Result: " + toolResult);
        }

        String finalReply = String(reply);
        if (toolResult != "") {
            finalReply += "\n[Tool: " + toolResult + "]";
        }
        return finalReply;
    } else {
        return "Error parsing my own thought... " + response;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n--- MicroClaw Firmware Starting ---");
    Serial.println("Author: Abhimanyu Singh (abhimanyus1997@gmail.com)");
    
    // Initialize File System
    fsManager.begin();
    
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

    claw.begin();
    tools = new Tools(&claw);
    
    // Initialize Web Server
    webServer = new WebInterface();
    
    // Connect to WiFi First (Important for TCP Stack)
    wifi->connect();

    // Bind agent logic to web server & Start
    webServer->begin([](String text) -> String {
        return handleAgentRequest(text);
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
