#ifndef GPIO_TOOLS_H
#define GPIO_TOOLS_H

#include <Arduino.h>

class GpioTools {
public:
    // Safe pins for ESP32 (excluding flash, input-only, etc depends on specific board but this is a general safe list for DevKit V1)
    // 2 (Builtin LED), 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33
    // Input only: 34, 35, 36, 39
    
    static bool isValidOutputPin(int pin) {
        int safe[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
        for (int p : safe) {
            if (p == pin) return true;
        }
        return false;
    }

    static bool isValidInputPin(int pin) {
        // Output pins + input only pins
        if (isValidOutputPin(pin)) return true;
        int inputs[] = {34, 35, 36, 39};
        for (int p : inputs) {
            if (p == pin) return true;
        }
        return false;
    }

    static String setPin(int pin, int value) {
        if (!isValidOutputPin(pin)) return "Error: Invalid Output Pin " + String(pin);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, value);
        return "Pin " + String(pin) + " set to " + (value ? "HIGH" : "LOW");
    }

    static String getPin(int pin) {
        if (!isValidInputPin(pin)) return "Error: Invalid Input Pin " + String(pin);
        pinMode(pin, INPUT);
        int val = digitalRead(pin);
        return String(val);
    }
};

#endif
