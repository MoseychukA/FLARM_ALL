#include "zTimesNRItalic36.h"
#include "zCalibri36.h"

#define AA_FONT_TIME zTimesNRItalic36
#define AA_FONT_CALI zCalibri36

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  tft.begin();
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);  // Set the font colour AND the background colour
  tft.setCursor(5, 5);                     // Set cursor at top left of screen

  tft.loadFont(AA_FONT_TIME);     // Must load the font first
  tft.println("Широкая электрификация южных губерний даст мощный толчок подъёму сельского хозяйства hkgfkhgfkfyyfutfluyffKFKFKUFYKFKFKF.");

  tft.loadFont(AA_FONT_CALI);     // Must load the font first
  tft.println("Широкая электрификация южных губерний даст мощный толчок подъёму сельского хозяйства.");
}

void loop() {
}
