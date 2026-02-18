#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <FS.h>
#include <LittleFS.h>

class FileSystem {
public:
    void begin() {
        if (!LittleFS.begin(true)) {
            Serial.println("LittleFS Mount Failed");
            return;
        }
        Serial.println("LittleFS Mounted");
    }

    String readFile(const char* path) {
        File file = LittleFS.open(path, "r");
        if (!file) {
            return "";
        }
        String content = file.readString();
        file.close();
        return content;
    }

    void writeFile(const char* path, const char* message) {
        File file = LittleFS.open(path, "w");
        if (!file) {
            Serial.println("Write failed");
            return;
        }
        file.print(message);
        file.close();
    }

    void appendFile(const char* path, const char* message) {
        File file = LittleFS.open(path, "a");
        if (!file) {
            Serial.println("Append failed");
            return;
        }
        file.print(message);
        file.close();
    }
};

extern FileSystem fsManager;

#endif
