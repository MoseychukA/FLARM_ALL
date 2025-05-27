/*
* Example provided by F1ATB
*
* See also https://github.com/AudunKodehode/JC3248W535EN-Touch-LCD
*/
#include <Arduino_GFX_Library.h>
#include <Wire.h>

// Pin definitions
#define GFX_BL 1
#define TOUCH_ADDR 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_I2C_CLOCK 400000
#define TOUCH_RST_PIN 12
#define TOUCH_INT_PIN 11
#define AXS_MAX_TOUCH_NUMBER 1

Arduino_DataBus* bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
Arduino_GFX* g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480); 
Arduino_Canvas* gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);

uint16_t touchX, touchY;

void setup() {
    Serial.begin(115200);
    Serial.println("Start setup!");
    // Initialize Display
    gfx->begin();

    // Initialize touch
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(TOUCH_I2C_CLOCK);

    // Configure touch pins
    pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
    pinMode(TOUCH_RST_PIN, OUTPUT);
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(200); 
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(200);

    gfx->setRotation(1);
    gfx->fillScreen(RGB565_BLUE);
    pinMode(GFX_BL, OUTPUT);  //Back Light On
    digitalWrite(GFX_BL, HIGH);
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_GREEN);
    gfx->setCursor(150, 10);
    gfx->print("Hello from F1ATB");
    gfx->setCursor(180, 100);
    gfx->print("Touch Screen");
    gfx->flush();
}

void loop() {
    if (getTouchPoint(touchX, touchY)) {
        Serial.println("Touch Pressed:" + String(touchX) + "," + String(touchY));
        gfx->fillRect(180, 100, 200, 20, RGB565_BLUE);
        gfx->setCursor(200, 100);
        gfx->print(String(touchX) + "," + String(touchY));
        gfx->drawCircle(touchX, touchY, 2, RGB565_WHITE);
        gfx->flush();
    }
    delay(5);
}

bool getTouchPoint(uint16_t& x, uint16_t& y) {
    uint8_t data[AXS_MAX_TOUCH_NUMBER * 6 + 2] = { 0 };

    // Define the read command array properly
    const uint8_t read_cmd[11] = {
        0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00,
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) >> 8),
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) & 0xff),
        0x00, 0x00, 0x00
    };

    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(read_cmd, 11);
    if (Wire.endTransmission() != 0) return false;

    if (Wire.requestFrom(TOUCH_ADDR, sizeof(data)) != sizeof(data)) return false;

    for (int i = 0; i < sizeof(data); i++) {
        data[i] = Wire.read();
    }

    if (data[1] > 0 && data[1] <= AXS_MAX_TOUCH_NUMBER) {
        uint16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
        uint16_t rawY = ((data[4] & 0x0F) << 8) | data[5];
        if (rawX > 500 || rawY > 500) return false;
        y = map(rawX, 0, 320, 320, 0);
        x = rawY;
        return true;
    }
    return false;
}