#include <Arduino.h>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#define PWM_PIN 48
#define PWM_CH  7

void setup() {
  Serial.begin(115200);
  pinMode(PWM_PIN, OUTPUT);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  bool ok = ledcAttachChannel(PWM_PIN, 10000, 12, PWM_CH);
  Serial.printf("ledcAttachChannel GPIO48: %s\r\n", ok ? "OK" : "FAIL");
  ledcWriteChannel(PWM_CH, 2048);
#else
  ledcSetup(PWM_CH, 10000, 12);
  ledcAttachPin(PWM_PIN, PWM_CH);
  ledcWrite(PWM_CH, 2048);
  Serial.println("LEDC GPIO48 core 2.x");
#endif
}

void loop() {
}
