#include "src/JC3248W535EN-Touch-LCD.h"
#include "Roboto_Regular16pt7b.h"
#include "ShortBaby_Mg2w16pt7b.h"

// Custom fonts can be made using this tool: https://rop.nl/truetype2gfx/
JC3248W535EN screen;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // Initialize the screen
  if (!screen.begin()) {
    Serial.println("Screen initialization failed!");
    return;
  }
  
  screen.clear(0, 0, 0);
  screen.setColor(255, 255, 255);
  
  // Display text with supported characters
  screen.setFont(&Roboto_Regular16pt7b);
  screen.prt("Roboto", 50, 50, 1);
  screen.setFont(&ShortBaby_Mg2w16pt7b);
  screen.prt("Shortbaby", 50, 100, 1);
  
  screen.flush();
}

void loop() {
  // Nothing to do in the loop
}