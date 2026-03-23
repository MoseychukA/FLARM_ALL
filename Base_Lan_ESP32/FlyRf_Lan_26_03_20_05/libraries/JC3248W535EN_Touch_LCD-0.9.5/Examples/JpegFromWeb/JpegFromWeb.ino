#include <WiFi.h>
#include <HTTPClient.h>
#include <JC3248W535EN-Touch-LCD.h>
#include <SPIFFS.h>

JC3248W535EN screen;

// WiFi credentials
const char* ssid = "SSID";
const char* password = "Password";

void setup() {
  Serial.begin(115200);

  // Initialize SPIFFS first
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
  } else {
    Serial.println("SPIFFS initialized successfully");
  }

  // Initialize the screen
  if (!screen.begin()) {
    Serial.println("Screen initialization failed!");
    return;
  }

  connectToWiFi();
  Serial.println("Loading image from the internet...");
  screen.loadImageFromUrl("https://github.com/AudunKodehode/JC3248W535EN-Touch-LCD/blob/main/Examples/JpegFromWeb/480x320.jpg?raw=true", 0, 0);
  screen.flush();
}

void connectToWiFi() {
  screen.prt("Connecting to WiFi...", 0, 0, 2);
  screen.flush();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
  }
  screen.clear(0, 0, 0);
}

void loop() {
}
