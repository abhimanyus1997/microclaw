#ifndef TOOLS_H
#define TOOLS_H

#include "common.h"
#include "wifi_tools.h"
#include "ble_tools.h"

class Tools {
public:
    Tools() {}

    // Execute a tool call based on name and arguments (JSON object)
    String execute(String toolName, JsonObject args) {
        Serial.print("Executing tool: ");
        Serial.println(toolName);

        if (toolName == "memory_write") {
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

        return "Unknown tool";
    }
};

#endif
