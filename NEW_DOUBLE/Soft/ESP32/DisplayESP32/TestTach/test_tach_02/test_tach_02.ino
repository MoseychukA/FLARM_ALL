#include "FT6336U.h"
#include "TFT_eSPI.h"      // 
#include <Wire.h>

#define I2C_SDA 8
#define I2C_SCL 9
#define RST_N_PIN 14
#define INT_N_PIN 47

#define TFT_WIDTH 320
#define TFT_HEIGHT 480
#define TOTAL_PAGES 8

TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT); // �������� ������� �������
FT6336U ft6336u(I2C_SDA, I2C_SCL, RST_N_PIN, INT_N_PIN);

int page = 0;
int startX, startY, endX, endY;
bool touchActive = false;

// ���������������� ��������
void drawPage(int num) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.drawString("PAGE " + String(num + 1), TFT_WIDTH / 2, TFT_HEIGHT / 2);
}



void setup() 
{
    Serial.begin(115200);
    tft.init();
    tft.setRotation(1); // ������� ��� ������ (0 - �������, 1 - ������)
    drawPage(page);

    ft6336u.begin();
 
}

void loop() 
{
    if (digitalRead(INT_N_PIN) == 0)
    {
 
        ft6336u.read_td_status();
        ft6336u.read_touch1_event();
        ft6336u.read_touch1_id();

        int y = ft6336u.read_touch1_x();   // 
        int x = ft6336u.read_touch1_y();  // 320 - �.�. �� ���-�����������

        if (!touchActive) 
        {
            // ����� �������
            startX = x; startY = y;
            touchActive = true;
        }
        endX = x; endY = y;
    }
    else if (touchActive/* && digitalRead(INT_N_PIN) != 0*/)
    {
        // ��c� ��������: ���������� �����
        int dx = endX - startX;

        if (abs(dx) > 50 && abs(endY - startY) < 80) 
        {
            if (dx > 0) 
            {
                // ����� ������ (��������� ��������)
             /*   page = (page + 1) % TOTAL_PAGES;*/
                page = (page - 1 + TOTAL_PAGES) % TOTAL_PAGES;
            }
            else 
            {
                // ����� ����� (���������� ��������)
               // page = (page - 1 + TOTAL_PAGES) % TOTAL_PAGES;
                page = (page + 1) % TOTAL_PAGES;
            }
            drawPage(page);
            Serial.printf("Page %d", page + 1);
            touchActive = false;
        }
    }

    //if(digitalRead(INT_N_PIN) == 0) 
    //{
    //    Serial.print("FT6336U TD Status: ");
    //    Serial.println(ft6336u.read_td_status());
    //    Serial.print("FT6336U Touch Event/ID 1: (");
    //    Serial.print(ft6336u.read_touch1_event()); Serial.print(" / "); Serial.print(ft6336u.read_touch1_id()); Serial.println(")");
    //    Serial.print("FT6336U Touch Position 1: (");
    //    Serial.print(ft6336u.read_touch1_x()); Serial.print(" , "); Serial.print(ft6336u.read_touch1_y()); Serial.println(")");
    //    Serial.print("FT6336U Touch Weight/MISC 1: (");
    //    Serial.print(ft6336u.read_touch1_weight()); Serial.print(" / "); Serial.print(ft6336u.read_touch1_misc()); Serial.println(")");
    //    Serial.print("FT6336U Touch Event/ID 2: (");
    //    Serial.print(ft6336u.read_touch2_event()); Serial.print(" / "); Serial.print(ft6336u.read_touch2_id()); Serial.println(")");
    //    Serial.print("FT6336U Touch Position 2: (");
    //    Serial.print(ft6336u.read_touch2_x()); Serial.print(" , "); Serial.print(ft6336u.read_touch2_y()); Serial.println(")");
    //    Serial.print("FT6336U Touch Weight/MISC 2: (");
    //    Serial.print(ft6336u.read_touch2_weight()); Serial.print(" / "); Serial.print(ft6336u.read_touch2_misc()); Serial.println(")");
    //}

}
