#ifndef SYSTEM_TOOLS_H
#define SYSTEM_TOOLS_H

#include <Arduino.h>
#include <ArduinoJson.h>

class SystemTools {
public:
    static String getSystemInfo() {
        StaticJsonDocument<512> doc;
        
        // Memory Info
        doc["heap_free"] = ESP.getFreeHeap();
        doc["heap_max_alloc"] = ESP.getMaxAllocHeap();
        doc["heap_min_free"] = ESP.getMinFreeHeap();
        
        // Flash Info
        doc["flash_size"] = ESP.getFlashChipSize();
        doc["flash_speed"] = ESP.getFlashChipSpeed();
        
        // CPU Info
        doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();
        
        // System Info
        doc["sdk_version"] = ESP.getSdkVersion();
        doc["uptime_seconds"] = millis() / 1000;
        
        // Sensors
        // hallRead() is removed in Arduino ESP32 Core v3.0+. 
        // We will skip internal sensors to ensure compatibility.
        
        // Internal Temp (Temperature Sensor is deprecated in newer IDFs and often inaccurate/requires config, skipping to ensure build stability)
        // doc["internal_temp"] = temperatureRead(); 

        String output;
        serializeJson(doc, output);
        return output;
    }
};

#endif
