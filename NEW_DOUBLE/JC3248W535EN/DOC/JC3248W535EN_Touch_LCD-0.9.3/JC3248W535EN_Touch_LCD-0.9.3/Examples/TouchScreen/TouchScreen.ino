#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;

uint16_t touchX, touchY;
void setup() {
  Serial.begin(115200);

  // Initialize the screen
  if (!screen.begin()) {
    Serial.println("Screen initialization failed!");
    return;
  }
  screen.clear(0, 0, 0);
    screen.setColor(255,255,255);
}

void loop() {
  if (screen.getTouchPoint(touchX, touchY)){
    Serial.println("Touch Pressed:" + String(touchX) + "," + String(touchY));
    screen.setColor(0,0,0);
    screen.drawFillRect(0, 0, 480, 20);
    screen.setColor(255,0,0);
    screen.prt((String(touchX) + "," + String(touchY)),0,0,2);
    screen.drawCircleOutline(touchX, touchY, 2);
  }
}
