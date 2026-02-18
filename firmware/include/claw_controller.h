#ifndef CLAW_CONTROLLER_H
#define CLAW_CONTROLLER_H

#include <ESP32Servo.h>

class ClawController {
public:
    ClawController(int pin) : _pin(pin) {
        _servo = new Servo();
    }

    void begin() {
        _servo->attach(_pin);
        open(); // Default state
    }

    void open() {
        Serial.println("Claw: Opening");
        _servo->write(180); // Adjust angle as per mechanic
    }

    void close() {
        Serial.println("Claw: Closing");
        _servo->write(0); // Adjust angle as per mechanic
    }

    void setAngle(int angle) {
         Serial.printf("Claw: Setting angle to %d\n", angle);
        _servo->write(angle);
    }

private:
    int _pin;
    Servo* _servo;
};

#endif
