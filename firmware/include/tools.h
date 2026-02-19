#ifndef TOOLS_H
#define TOOLS_H

#include "common.h"
#include "wifi_tools.h"
#include "ble_tools.h"

class Tools {
public:
    Tools() {}

    // Payload for the task
    struct ScriptTaskParams {
        String scriptJson;
    };

    // FreeRTOS Task to run the script
    static void scriptTask(void* parameter) {
        ScriptTaskParams* params = (ScriptTaskParams*)parameter;
        DynamicJsonDocument doc(4096);
        deserializeJson(doc, params->scriptJson);
        JsonArray script = doc.as<JsonArray>();

        for (JsonVariant v : script) {
            JsonObject cmd = v.as<JsonObject>();
            String type = cmd["cmd"].as<String>();
            
            if (type == "gpio") {
                int pin = cmd["pin"];
                int state = cmd["state"];
                pinMode(pin, OUTPUT);
                digitalWrite(pin, state ? HIGH : LOW);
            }
            else if (type == "delay") {
                int ms = cmd["ms"];
                delay(ms); // Task delay, doesn't block system
            }
            else if (type == "loop") {
                int count = cmd["count"];
                JsonArray steps = cmd["steps"].as<JsonArray>();
                
                // Recursively running loops is tricky with flat JSON. 
                // We'll re-serialize steps to handle it recursively or just handle 1 level safely.
                // For simplicity/safety in this structure, let's just re-use a helper method
                // BUT we can't call runScript (it launches a task).
                // We need a blocking helper for the *internal* execution of the task.
                executeScriptInternal(steps); 
            }
        }
        
        // Cleanup
        delete params;
        vTaskDelete(NULL);
    }

    // Helper for internal blocking execution (recursion support)
    static void executeScriptInternal(JsonArray script) {
         for (JsonVariant v : script) {
            JsonObject cmd = v.as<JsonObject>();
            String type = cmd["cmd"].as<String>();
            
            if (type == "gpio") {
                int pin = cmd["pin"];
                int state = cmd["state"];
                pinMode(pin, OUTPUT);
                digitalWrite(pin, state ? HIGH : LOW);
            }
            else if (type == "delay") {
                int ms = cmd["ms"];
                delay(ms);
            }
            // Loop support max 1 level of recursion to avoid stack overflow in task
            // or we could allow it but need to be careful.
             else if (type == "loop") {
                int count = cmd["count"];
                JsonArray steps = cmd["steps"].as<JsonArray>();
                for(int i=0; i<count; i++) {
                    executeScriptInternal(steps);
                }
             }
         }
    }

    // New runScript launches the task
    String runScript(JsonArray script) {
        if (script.isNull()) return "Invalid script";
        
        // Serialize script to string to pass to task
        String scriptJson;
        serializeJson(script, scriptJson);
        
        ScriptTaskParams* params = new ScriptTaskParams();
        params->scriptJson = scriptJson;

        // Launch Task on Core 1 (Application Core) or Core 0. Arduino runs on 1 usually.
        // Stack size 8192 should be plenty for JSON parsing.
        xTaskCreate(
            scriptTask,     // Function
            "ScriptTask",   // Name
            8192,           // Stack size
            params,         // Parameters
            1,              // Priority
            NULL            // Handle
        );
        
        return "Script started in background";
    }

    // Execute a tool call based on name and arguments (JSON object)
    String execute(String toolName, JsonObject args) {
        Serial.print("Executing tool: ");
        Serial.println(toolName);

        if (toolName == "run_script") {
            return runScript(args["script"].as<JsonArray>());
        }
        else if (toolName == "memory_write") {
            const char* content = args["content"];
            if (content) {
                fsManager.appendFile("/MEMORY.md", content);
                fsManager.appendFile("/MEMORY.md", "\n");
                return "Memory updated";
            }
            return "No content provided";
        }
        else if (toolName == "memory_read") {
            String mem = fsManager.readFile("/MEMORY.md");
            if (mem == "") return "Memory is empty";
            return mem;
        }
        else if (toolName == "get_system_stats") {
            return SystemTools::getSystemInfo();
        }
        else if (toolName == "gpio_control") {
            int pin = args["pin"];
            const char* mode = args["mode"];
            if (String(mode) == "output") {
                int state = args["state"];
                return GpioTools::setPin(pin, state);
            } else {
                return GpioTools::getPin(pin);
            }
        }
        else if (toolName == "wifi_scan") {
            return WifiTools::scan();
        }
        else if (toolName == "ble_scan") {
            return BleTools::scan();
        }
        else if (toolName == "ble_connect") {
            const char* addr = args["address"];
            if (addr) return BleTools::connect(String(addr));
            return "Address required";
        }
        else if (toolName == "ble_disconnect") {
            return BleTools::disconnect();
        }

        return "Unknown tool";
    }
};

#endif
