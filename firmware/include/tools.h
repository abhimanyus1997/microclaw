#ifndef TOOLS_H
#define TOOLS_H

#include "claw_controller.h"
#include "file_system.h"
#include <ArduinoJson.h>

class Tools {
public:
    Tools(ClawController* claw) : _claw(claw) {}

    // Execute a tool call based on name and arguments (JSON object)
    String execute(String toolName, JsonObject args) {
        Serial.print("Executing tool: ");
        Serial.println(toolName);

        if (toolName == "claw_control") {
            String action = args["action"].as<String>();
            if (action == "OPEN") {
                _claw->open();
                return "Claw opened";
            } else if (action == "CLOSE") {
                _claw->close();
                return "Claw closed";
            } else if (action == "WAVE") {
                _claw->open(); delay(300);
                _claw->close(); delay(300);
                _claw->open();
                return "Claw waved";
            } else {
                return "Unknown claw action";
            }
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

        return "Unknown tool";
    }

private:
    ClawController* _claw;
};

#endif
