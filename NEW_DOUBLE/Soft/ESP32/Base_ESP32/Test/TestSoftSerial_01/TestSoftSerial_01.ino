#include <Arduino.h>
#include <SoftwareSerial.h>

// Define SoftwareSerial
SoftwareSerial mySerial(38, 39);

void setup() {
    // Initialize hardware serial baud Rate
    Serial.begin(19200);
    mySerial.begin(19200);
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        // Send data received from hardware serial to software serial
        mySerial.write(c);
    }

    if (mySerial.available()) {
        char c = mySerial.read();
        // Send data received from software serial to hardware serial
        Serial.write(c);
    }
}