# JC3248W535EN Touch LCD Library

A simple, lightweight library to drive the JC3248W535EN touch LCD display from Guition without requiring LVGL. This library provides an easy way to get started with your display using basic drawing functions and touch support.

Display: https://www.aliexpress.com/item/1005007566315926.html

## Overview

Many users struggle to get started with the JC3248W535EN display after purchasing it. This library serves as an excellent starting point, abstracting away the complex initialization and providing an intuitive API for common display operations.

## Features

- Easy display initialization
- Basic drawing primitives (rectangles, circles, triangles, lines, etc.)
- Text rendering
- Touch input handling
- QR code generation and display
- Portrait orientation support
- Example with command-line screen-design feature, featuring a python GUI designer.

## Dependencies

To use this library, you'll need to install:

1. [Arduino_GFX_Library](https://github.com/moononournation/Arduino_GFX_Library) - For the display graphics
2. Arduino Wire library (built into Arduino IDE)
3. Arduino ESP32 board support

## Installation

### Using Arduino IDE Library Manager (Recommended)

1. Open Arduino IDE
2. Go to Sketch -> Include Library -> Manage Libraries...
3. Search for "JC3248W535EN"
4. Click Install

### Manual Installation

1. Download this repository as ZIP
2. In Arduino IDE, go to Sketch -> Include Library -> Add .ZIP Library...
3. Select the downloaded ZIP file
4. Restart Arduino IDE

### Required Dependencies

Install these libraries using Arduino Library Manager:
1. [Arduino_GFX_Library](https://github.com/moononournation/Arduino_GFX_Library)

Arduino IDE Setup: 
For the JC3248W535EN, use ESP332S3 Dev module board. 
Set up as follow: 

USB CDC On Boot: "Enabled" // Important for Serial communication
CPU Frequency: "240MHz (WiFi)"
Core Debug Level: "None"
USB DFU On Boot: "Disabled"
Erase All Flash Before Sketch Upload: "Disabled"
Events Run On: "Core 1"
Flash Mode: "QIO 80MHz"
Flash Size: "16MB (128Mb)"
JTAG Adapter: "Disabled"
Arduino Runs On: "Core 1"
USB Firmware MSC On Boot: "Disabled"
Partition Scheme: "8M with spiffs (3MB APP/1.5MB SPIFFS)"
PSRAM: "OPI PSRAM"
Upload Mode: "UART0 / Hardware CDC"
Upload Speed: "921600"
USB Mode: "Hardware CDC and JTAG"
Zigbee Mode: "Disabled"


Tools 

## Usage Examples

### Basic Initialization

```arduino
#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;

void setup() {
  Serial.begin(115200);

  // Initialize the screen
  if (!screen.begin()) {
    Serial.println("Screen initialization failed!");
    return;
  }
  
  // Clear the screen with white background
  screen.clear(255, 255, 255);
}

void loop() {
  // Your code here
}
```

### Drawing Shapes and Text

```arduino
#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;

void setup() {
  Serial.begin(115200);
  screen.begin();
  screen.clear(255, 255, 255);
  
  // Draw a blue filled rectangle
  screen.setColor(0, 0, 255);
  screen.drawFillRect(50, 50, 100, 80);
  
  // Draw a red filled circle
  screen.setColor(255, 0, 0);
  screen.drawFillCircle(200, 150, 40);
  
  // Draw text
  screen.setColor(0, 0, 0);
  screen.prt("Hello World!", 120, 250, 2);
}

void loop() {
}
```

### Touch Input Example

```arduino
#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;
uint16_t touchX, touchY;

void setup() {
  Serial.begin(115200);
  screen.begin();
  screen.clear(255, 255, 255);
  screen.setColor(0, 0, 0);
  screen.prt("Touch the screen!", 80, 200, 2);
}

void loop() {
  if (screen.getTouchPoint(touchX, touchY)) {
    // Draw a small circle where touch is detected
    screen.setColor(255, 0, 0);
    screen.drawFillCircle(touchX, touchY, 5);
    
    // Display coordinates
    screen.setColor(0, 0, 0);
    String coords = "X: " + String(touchX) + " Y: " + String(touchY);
    screen.clear(255, 255, 255);
    screen.prt(coords, 100, 240, 2);
    
    delay(100);  // Small delay to prevent too many readings
  }
}
```

### QR Code Generation

```arduino
#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;

void setup() {
  Serial.begin(115200);
  screen.begin();
  screen.clear(255, 255, 255);
  
  // Draw a QR code (centered on screen)
  screen.drawQRCode("https://github.com/yourusername/JC3248W535EN-Touch-LCD", 
                   60, 120, 4, 255, 255, 255, 0, 0, 0);
                   
  screen.setColor(0, 0, 0);
  screen.prt("Scan me!", 120, 60, 2);
}

void loop() {
}
```

## Pin Configuration

This library uses the following default pin configuration for the JC3248W535EN display:

- Backlight: Pin 1
- Touch SDA: Pin 4
- Touch SCL: Pin 8
- Touch RST: Pin 12
- Touch INT: Pin 11

## Screen Orientation

The library handles the display in portrait orientation with proper coordinate mapping for ease of use.

## Contributing

Contributions to improve the library are welcome! Please feel free to submit pull requests.

## License

This project is released under the MIT License.