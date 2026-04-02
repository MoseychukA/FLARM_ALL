#include <Arduino.h>
#include "ESP32RF.h"
#include "TrafficDB.h"
#include "RF.h"
#include "FlarmDecoder.h"
#include "RP2040Bridge.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "LANRF.h"
#include "RS485Display.h"
#include <esp_task_wdt.h>
#include "Container.h"
#include "DeviceInfo.h"
#include "FlyRfSpi.h"
#include <TFT_eSPI.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

//#include "Bluetooth.h"
#include "fonts/NotoSansMonoSCB20.h"
#include "fonts/NotoSansBold15.h"

#include "fonts/zTimesNRItalic28.h"
#include "fonts/zCalibri36.h"
#include "fonts/zTimesNR28.h"
#include "fonts/zTimesNR14.h"
#include "fonts/zTimesNR18.h"
#include "fonts/zTimesNR24.h"

#define AA_FONT_CALI zCalibri36
#define AA_FONT_TIME28I zTimesNRItalic28
#define AA_FONT_TIME28 zTimesNR28
#define AA_FONT_TIME18 zTimesNR18
#define AA_FONT_TIME24 zTimesNR24

#define WIDTH  480
#define HEIGHT 480

#define INFO_HEIGHT 17 


// create a new sprite
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back = TFT_eSprite(&tft);               // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite rows_Message = TFT_eSprite(&tft);       // Спрайт отображения текстов сообщений
TFT_eSprite location_Message = TFT_eSprite(&tft);   // Спрайт отображения текстов сообщений
TFT_eSprite state_SOS_button = TFT_eSprite(&tft);   // Спрайт отображения состояния кнопки SOS
TFT_eSprite time_info = TFT_eSprite(&tft);          // Этот спрайт, площадка в котором будет располагатся информация о времени с GPS
TFT_eSprite base_connection = TFT_eSprite(&tft);    // Этот спрайт, площадка в котором будет располагатся информация о связи с базой
TFT_eSprite info_header = TFT_eSprite(&tft);        // Этот спрайт, площадка в котором будет располагатся заголовок информации 

TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
TFT_eSprite* airplane[MAX_TRACKING_OBJECTS];        // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета DUMP1090
TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся спрайт airplane стороннего самолета
TFT_eSprite power1 = TFT_eSprite(&tft);             // Спрайт отображения заряда аккумулятора 
TFT_eSprite voltage1 = TFT_eSprite(&tft);           // Спрайт отображения напряжения аккумулятора 
TFT_eSprite current1 = TFT_eSprite(&tft);           // Спрайт отображения ток потребления 
TFT_eSprite* airplane_alient_info[MAX_TRACKING_OBJECTS];

int alien_altitude_old[MAX_TRACKING_OBJECTS];       // Предыдущее значение высоты стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int alien_altitude_actual[MAX_TRACKING_OBJECTS];    // Высота стороннего самолета. Нужно для вычисления высоты с учетом гистерезиса
int this_alien_altitude[MAX_TRACKING_OBJECTS];      // Высота стороннего самолета. Нужно для вычисления 
int old_alien_altitude_arrow[MAX_TRACKING_OBJECTS]; // Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
int alien_altitude_hysteresis[MAX_TRACKING_OBJECTS];// Обработанная высота стороннего самолета после применения гистерезиса
int height_difference[MAX_TRACKING_OBJECTS];        // Разность высот нашего и стороннего самолета
int alien_speed_tmr[MAX_TRACKING_OBJECTS];          // Скорость стороннего самолета
int alien_speed_view[MAX_TRACKING_OBJECTS];         // Скорость стороннего самолета для вывода на дисплей в виде линии
int alient_course[MAX_TRACKING_OBJECTS];            // Курс стороннего самолета
int Container_alien_X[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
int Container_alien_Y[MAX_TRACKING_OBJECTS];        // Координаты стороннего самолета
int Container_logbook_X[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
int Container_logbook_Y[MAX_TRACKING_OBJECTS];      // Координаты формуляра стороннего самолета
uint8_t arrow_up_down[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
uint8_t arrow_up_down_old[MAX_TRACKING_OBJECTS];    // флаг стрелки вверх или вниз
bool Air_txt_left[MAX_TRACKING_OBJECTS];            // флаг расположения формуляра слева или справа

word  little_air_color[MAX_TRACKING_OBJECTS];                      // Цвет предупреждения столкновения с сторонним самолетом 
int alien_speed_filtre[MAX_TRACKING_OBJECTS][speed_array_size];    // Фильтр скорости стороннего самолета
int alien_altitude_filtre[MAX_TRACKING_OBJECTS][speed_array_size]; // Фильтр высоты стороннего самолета
bool alien_speed_array_countMax[MAX_TRACKING_OBJECTS];             // Флаг заполнения массиво фильтра скорости стороннего самолета
int alien_speed_sum[MAX_TRACKING_OBJECTS];                         // = 0;
uint8_t alien_speed_array_count[MAX_TRACKING_OBJECTS];             // Счетчик фильтра скорости стороннего самолета
bool alien_altitude_array_countMax[MAX_TRACKING_OBJECTS];          // Флаг заполнения массива фильтра высоты стороннего самолета
int alien_altitude_sum[MAX_TRACKING_OBJECTS];                      // = 0;
uint8_t alien_altitude_array_count[MAX_TRACKING_OBJECTS];          // Счетчик фильтра высоты стороннего самолета
static uint32_t tmr_array[MAX_TRACKING_OBJECTS];                   // Задержка по времени контроля движения чужого самолета
int16_t new_angle[MAX_TRACKING_OBJECTS];                           // Для вычисления курса стороннего самолета
bool isTeam_all[MAX_TRACKING_OBJECTS] = { false };                 // Удалить данные по самолету

//......................................colors
#define backColor  0x0026

//===================================================================================
static constexpr int16_t kRadarPivotX = 240;
static constexpr int16_t kRadarPivotY = 240;
static constexpr int16_t kRadarCompassOuterRadiusPx = 233;
static constexpr int16_t kRadarCompassMajorInnerRadiusPx = 224;
static constexpr int16_t kRadarCompassMinorInnerRadiusPx = 229;
static constexpr int16_t kRadarCompassLabelRadiusPx = 232;
static constexpr int16_t kRadarCompassNeedleRadiusPx = 202;
static constexpr int16_t kRadarRingRadiusPx = 120;
static constexpr int16_t kRadarTargetOuterRadiusPx = 224;
static constexpr int16_t kRadarOwnshipCenterX = kRadarPivotX;
static constexpr int16_t kRadarOwnshipCenterY = kRadarPivotY;
static constexpr int16_t kRadarRangeLabelX = kRadarPivotX + 6;
static constexpr int16_t kRadarRangeLabelY = 303;

int nc = 0;

float fx[360]; //outer points of Speed gaouges
float fy[360];
float px[360]; //ineer point of Speed gaouges
float py[360];
float px1[360]; //ineer point of Speed gaouges
float py1[360];
float lx[360]; //text of Speed gaouges
float ly[360];
float nx[360]; //needle low of Speed gaouges
float ny[360];

double rad = 0.01745;
int angle = 0;
int angle_old = 0;


int thisAircraft_altitude_tmr = 0;       // ThisAircraft
int thisAircraft_speed_tmr = 0;
int thisAircraft_course_tmr = 0;

#ifndef DISPLAY_BUTTON_PIN
#define DISPLAY_BUTTON_PIN 48
#endif

#ifndef DISPLAY_BUTTON_ACTIVE_LEVEL
#define DISPLAY_BUTTON_ACTIVE_LEVEL LOW
#endif

#ifndef DISPLAY_BUTTON_DEBOUNCE_MS
#define DISPLAY_BUTTON_DEBOUNCE_MS 30UL
#endif

#ifndef DISPLAY_BUTTON_DOUBLE_MS
#define DISPLAY_BUTTON_DOUBLE_MS 350UL
#endif

#ifndef DISPLAY_BUTTON_LONG_MS
#define DISPLAY_BUTTON_LONG_MS 900UL
#endif

static volatile uint8_t g_pendingDisplayButton = 0;
static TaskHandle_t g_displayButtonTask = nullptr;

static uint8_t takeDisplayButtonEvent()
{
    if (RS485Display_hasIncomingButton())
    {
        const uint8_t rs485Event = RS485Display_takeIncomingButton();
        if (rs485Event != 0)
        {
            g_pendingDisplayButton = 0;
            return rs485Event;
        }
    }
    const uint8_t event = g_pendingDisplayButton;
    g_pendingDisplayButton = 0;
    return event;
}

static void pushDisplayButtonEvent(uint8_t event)
{
    if (event == 0) return;
    g_pendingDisplayButton = event;
}

static void button(void* param)
{
    (void)param;
    pinMode(DISPLAY_BUTTON_PIN, INPUT_PULLUP);

    bool stableState = (digitalRead(DISPLAY_BUTTON_PIN) == DISPLAY_BUTTON_ACTIVE_LEVEL);
    bool lastRawState = stableState;
    bool pressActive = false;
    bool longSent = false;
    bool waitSecondClick = false;
    uint32_t rawChangedAt = millis();
    uint32_t pressedAt = 0;
    uint32_t firstReleaseAt = 0;

    for (;;)
    {
        const uint32_t now = millis();
        const bool rawState = (digitalRead(DISPLAY_BUTTON_PIN) == DISPLAY_BUTTON_ACTIVE_LEVEL);

        if (rawState != lastRawState)
        {
            lastRawState = rawState;
            rawChangedAt = now;
        }

        if ((now - rawChangedAt) >= DISPLAY_BUTTON_DEBOUNCE_MS && rawState != stableState)
        {
            stableState = rawState;

            if (stableState)
            {
                pressActive = true;
                longSent = false;
                pressedAt = now;
            }
            else if (pressActive)
            {
                pressActive = false;
                if (!longSent)
                {
                    if (waitSecondClick && (now - firstReleaseAt) <= DISPLAY_BUTTON_DOUBLE_MS)
                    {
                        waitSecondClick = false;
                        pushDisplayButtonEvent(2);
                    }
                    else
                    {
                        waitSecondClick = true;
                        firstReleaseAt = now;
                    }
                }
            }
        }

        if (pressActive && !longSent && (now - pressedAt) >= DISPLAY_BUTTON_LONG_MS)
        {
            longSent = true;
            waitSecondClick = false;
            pushDisplayButtonEvent(3);
        }

        if (waitSecondClick && !pressActive && (now - firstReleaseAt) > DISPLAY_BUTTON_DOUBLE_MS)
        {
            waitSecondClick = false;
            pushDisplayButtonEvent(1);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

uint8_t index_nearest_aircraft = 0;      // индекс ближайшего самолета
uint8_t  set_view_range = 0;
uint8_t  view_alien_count = 0;                // Переменная для определения количества сторонних самолетов.

int16_t alient_course0 = 0;              // Курс ближайшего стороннего самолета
int16_t alient_speed0 = 0;              // Скорость ближайшего стороннего самолета
int8_t  txt_loc_speed = 87;             // место вывода текста скорости стороннегосамолета

int32_t divider = 2000;                  //делитель равен половине полной шкалы
int divider_num = 1;

int16_t new_x;
int16_t new_y;

int16_t form_x = 0;
int16_t form_y = 0;
int16_t form_arrow_x = 0;
int16_t form_arrow_y = 0;

int test_curse = 0;
float Aircraft_latitude_old1 = 0;
float Aircraft_longitude_old1 = 0;
uint8_t  fix_tmp = false;

static bool TFT_display_frontpage = false;


//====================================================================================

lmic_pinmap lmic_pins = {
    .nss = GPIO_PIN_SS,
    .txe = LMIC_UNUSED_PIN,
    .rxe = LMIC_UNUSED_PIN,
    .rst = GPIO_PIN_RST,
    .dio = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
    .busy = LMIC_UNUSED_PIN, //SOC_GPIO_PIN_BUSY,//!!SOC_GPIO_PIN_TXE,
    .tcxo = LMIC_UNUSED_PIN,
};

const uint8_t whitening_pattern[] PROGMEM = { 0x05, 0xb4, 0x05, 0xae, 0x14, 0xda,
  0xbf, 0x83, 0xc4, 0x04, 0xb2, 0x04, 0xd6, 0x4d, 0x87, 0xe2, 0x01, 0xa3, 0x26,
  0xac, 0xbb, 0x63, 0xf1, 0x01, 0xca, 0x07, 0xbd, 0xaf, 0x60, 0xc8, 0x12, 0xed,
  0x04, 0xbc, 0xf6, 0x12, 0x2c, 0x01, 0xd9, 0x04, 0xb1, 0xd5, 0x03, 0xab, 0x06,
  0xcf, 0x08, 0xe6, 0xf2, 0x07, 0xd0, 0x12, 0xc2, 0x09, 0x34, 0x20 };

extern const uint8_t whitening_pattern[] PROGMEM;


static const char* sourceToText(TrafficSource source)
{
    switch (source)
    {
    case TRAFFIC_SOURCE_FLARM_LORA: return "FLARM";
    case TRAFFIC_SOURCE_ADSB_DUMP1090: return "ADSB";
    default: return "UNK";
    }
}


void Display_setup()
{
    if (g_displayButtonTask == nullptr)
    {
        xTaskCreatePinnedToCore(button, "Button", 2048, NULL, 2, &g_displayButtonTask, 1);
    }

   // byte rval = DISPLAY_NONE;

    {
        FlyRfSpiGuard spiGuard(500);
        if (spiGuard)
        {
            tft.begin();
            tft.setRotation(3);
            tft.fillScreen(TFT_NAVY);
        }
    }
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Set the font colour AND the background colour


    uint16_t tbw1;
    uint16_t x_tft, y_tft;

    //tft.setTextWrap(false);
    tft.loadFont(AA_FONT_CALI);     // Must load the font first

    tft.setCursor(170, 80);
    tft.println("FlyRF Lan");
    tft.setCursor(170, 120);
    tft.println("DECIMA");

    tft.loadFont(AA_FONT_TIME28I);     // Must load the font first
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);  // Set the font colour AND the background colour
    tft.setCursor(140, 180);
    tft.println("ВКЛЮЧАЕТСЯ");
    tft.setCursor(155, 220);
    tft.println("ОЖИДАЙТЕ");


    tft.loadFont(AA_FONT_TIME18);     // Must load the font first
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Set the font colour AND the background colour

    tft.setCursor(10, 275);
    tft.println("(C) 2026");

    tft.setCursor(10, 295);
    tft.println("www.decima.ru");

    String Current_version = DeviceInfo_programVersion();
    tbw1 = tft.textWidth(Current_version);
    x_tft = (tft.width() - tbw1) - 4;

    y_tft = tft.height() - tft.fontHeight() + 10;
    tft.setCursor(x_tft, 295);
    tft.print(Current_version);

    back.setColorDepth(8);
    back.createSprite(480, 480);


    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        Air_txt_Sprite[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта
        Air_txt_Sprite[i]->createSprite(55, 15);
        Air_txt_Sprite[i]->setPivot(27, 7);

        airplane[i] = new TFT_eSprite(&tft);           // Спрайт информации стороннего воздушного объекта
        airplane[i]->createSprite(100, 100);           // Спрайт отображения объекта, полученного из DUMP1090. 
        airplane[i]->setPivot(50, 50);

        area_airplane[i] = new TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся сторонний самолет
        area_airplane[i]->createSprite(100, 100);
        area_airplane[i]->setPivot(50, 50);

        alien_speed_array_countMax[i] = false;
        alien_speed_sum[i] = 0;
        alien_speed_array_count[i] = 0;

        alien_altitude_array_countMax[i] = false;
        alien_altitude_sum[i] = 0;
        alien_altitude_array_count[i] = 0;

        old_alien_altitude_arrow[i] = 0;                // Массив хранения предыдущих значений высоты, для формирования стрелок направления перемещения самолета вврх/вниз

        tmr_array[i] = millis();

        airplane_alient_info[i] = new TFT_eSprite(&tft);
        airplane_alient_info[i]->createSprite(480, INFO_HEIGHT);
        airplane_alient_info[i]->fillSprite(backColor);
        airplane_alient_info[i]->setTextColor(TFT_WHITE, backColor);
        airplane_alient_info[i]->setTextDatum(TL_DATUM);
        airplane_alient_info[i]->setFreeFont(&FreeSerif9pt7b);
        airplane_alient_info[i]->setTextSize(0);
        esp_task_wdt_reset();
    }

    power1.createSprite(74, 20);
    time_info.createSprite(60, 25);

    backsprite.createSprite(480, 480);
    backsprite.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
    backsprite.setSwapBytes(true);
    backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
    backsprite.setTextDatum(4);
    backsprite.setPivot(kRadarPivotX, kRadarPivotY); // Назначаем центр вращения спрайта воздушной обстановки

    int a = 270;
    for (int i = 0; i < 360; i++)
    {
        fx[i] = (kRadarCompassOuterRadiusPx * cos(rad * a)) + kRadarPivotX;          // внешняя точка крупной метки
        fy[i] = (kRadarCompassOuterRadiusPx * sin(rad * a)) + kRadarPivotY;
        px[i] = (kRadarCompassMajorInnerRadiusPx * cos(rad * a)) + kRadarPivotX;     // внутренняя точка крупной метки
        py[i] = (kRadarCompassMajorInnerRadiusPx * sin(rad * a)) + kRadarPivotY;
        px1[i] = (kRadarCompassMinorInnerRadiusPx * cos(rad * a)) + kRadarPivotX;     // внутренняя точка малой метки
        py1[i] = (kRadarCompassMinorInnerRadiusPx * sin(rad * a)) + kRadarPivotY;
        lx[i] = (kRadarCompassLabelRadiusPx * cos(rad * a)) + kRadarPivotX;           // положение символов направления
        ly[i] = (kRadarCompassLabelRadiusPx * sin(rad * a)) + kRadarPivotY;
        nx[i] = (kRadarCompassNeedleRadiusPx * cos(rad * a)) + kRadarPivotX;
        ny[i] = (kRadarCompassNeedleRadiusPx * sin(rad * a)) + kRadarPivotY;

        a++;
        if (a == 360)
            a = 0;
    }
}


//========================= Функции вывода информации о сторонних объектах на экран ==================

static bool block_plane_display = false;
int line_Sprite = 0;                      // текущая линия для вывода (строк без дыр)
static int prev_lines_count = 0;

// Вызывать эту функцию, чтобы полностью скрыть и отключить вывод авиа-треков и заголовка
void clearPlaneDisplay()
{
    // 1. Очистить все авиа-спрайты, не удаляя их из памяти
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (airplane_alient_info[i]->created())
        {
            airplane_alient_info[i]->fillSprite(TFT_BLACK);
            airplane_alient_info[i]->pushToSprite(&back, 0, 0, TFT_BLACK);
        }
    }

    // 2. Очистить заголовок
    if (info_header.created())
    {
        info_header.fillSprite(TFT_BLACK);
        info_header.pushToSprite(&back, 0, 0, TFT_BLACK);
    }

    // 4. Сбросить/обнулить служебную переменную строк (если она есть)

    prev_lines_count = 0;

    // 5. Запретить дальнейший вывод
    block_plane_display = true;
}

// Чтобы разрешить обратно отрисовку авиа-треков:
void enablePlaneDisplay()
{
    block_plane_display = false;
}


void displayAllPlanes() // Вывести текстовую информацию по самолетам на дисплей
{
    if (block_plane_display)
        return; // Блокировка активна — ничего не выводить!

    line_Sprite = 0;
    bool has_data_to_display = false;

    // 1. Проверяем: есть ли что выводить?
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container[i].addr != 0)
        {
            if (settings->display_set == INFO_DISPLAY_COORDINATE)
            {
                if (Container[i].latitude != 0 && Container[i].longitude != 0)
                {
                    has_data_to_display = true;
                    break;
                }
            }
            else if (settings->display_set == INFO_DISPLAY_MAXI)
            {
                has_data_to_display = true;
                break;
            }
        }
    }

    // 2. Управляем заголовком: создаём/рисуем или удаляем
    if (has_data_to_display)
    {
        if (!info_header.created())
        {
            info_header.createSprite(480, 20);
            info_header.setTextDatum(TL_DATUM);
            info_header.setTextColor(TFT_WHITE, backColor);
            info_header.setFreeFont(&FreeSerif9pt7b);
        }
        info_header.fillSprite(backColor);
        info_header.drawString("  ICAO  |SQUAWK| FLIGHT |  ALT | SPEED |COURSE|LATITUDE|LONGITUDE| SIG", 0, 1, 2);
        info_header.drawLine(0, 18, 479, 18, TFT_WHITE);
        info_header.pushToSprite(&back, 0, 22);
    }
    else
    {
        if (info_header.created())
        {
            info_header.fillSprite(TFT_BLACK);
            info_header.pushToSprite(&back, 0, 22);
        }
    }

    // 3. Основной цикл по объектам
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container[i].addr != 0)
        {
            bool need_draw = false;
            if (settings->display_set == INFO_DISPLAY_COORDINATE)
            {
                if (Container[i].latitude != 0 && Container[i].longitude != 0)
                    need_draw = true;
            }
            else if (settings->display_set == INFO_DISPLAY_MAXI)
            {
                need_draw = true;
            }

            if (need_draw)
            {
                // Спрайт строки уже создан заранее в setup()

                // Заполняем данными
                airplane_alient_info[i]->fillSprite(backColor);

                String addrStr = String(Container[i].addr, HEX);
                addrStr.toUpperCase(); // перевели в ВЕРХНИЙ регистр
                airplane_alient_info[i]->drawString(addrStr, 3, 2, 2);
                if (Container[i].squawk != 0)
                {
                    airplane_alient_info[i]->drawString(String(Container[i].squawk), 65, 2, 2);
                }
                airplane_alient_info[i]->drawString(Container[i].callsign, 107, 2, 2);
                if ((int)Container[i].altitude != 0)
                {
                    airplane_alient_info[i]->drawString(String((int)Container[i].altitude), 170, 2, 2);
                }
                if ((int)Container[i].speed != 0)
                {
                    airplane_alient_info[i]->drawString(String((int)Container[i].speed), 225, 2, 2);
                }
                if ((int)Container[i].course != 0)
                {
                    airplane_alient_info[i]->drawString(String((int)Container[i].course), 275, 2, 2);
                }
                if (Container[i].latitude != 0)
                {
                    airplane_alient_info[i]->drawString(String(Container[i].latitude, 5), 315, 2, 2);
                }
                if (Container[i].longitude != 0)
                {
                    airplane_alient_info[i]->drawString(String(Container[i].longitude, 5), 383, 2, 2);
                }

                if (Container[i].signal_source == 1)
                {
                    if (Container[i].rssi_LoRa == 0)
                       airplane_alient_info[i]->drawString("--", 458, 2, 2);
                    // airplane_alient_info[i]->drawString(String(Container[i].rssi_LoRa), 462, 2, 2);
                    else
                        airplane_alient_info[i]->drawString(String(Container[i].rssi_LoRa), 450, 2, 2);
                }
                else if (Container[i].signal_source == 2)
                {
                     if (Container[i].rssi_rp2040 == 0)
                        airplane_alient_info[i]->drawString("--", 458, 2, 2);
                    // airplane_alient_info[i]->drawString(String(Container[i].rssi_rp2040), 462, 2, 2);
                    else
                        airplane_alient_info[i]->drawString(String(Container[i].rssi_rp2040), 450, 2, 2);
                }

                // Отрисовываем спрайт строки (смещение учитывает заголовок)
                airplane_alient_info[i]->pushToSprite(&back, 0, 41 + (line_Sprite * INFO_HEIGHT));

                line_Sprite++;
            }
            else
            {
                if (airplane_alient_info[i]->created())
                {
                    airplane_alient_info[i]->fillSprite(TFT_BLACK);
                }
            }
        }
        else
        {
            if (airplane_alient_info[i]->created())
            {
                airplane_alient_info[i]->fillSprite(TFT_BLACK);
            }
        }
    }

    // 4. Очищаем только лишние строки (на случай уменьшения числа треков)
    if (prev_lines_count > line_Sprite)
    {
        for (int l = line_Sprite; l < prev_lines_count; l++)
        {
            FlyRfSpiGuard spiGuard(50);
            if (spiGuard)
            {
                tft.fillRect(0, 41 + (l * INFO_HEIGHT), WIDTH, INFO_HEIGHT, TFT_BLACK);
            }
        }
    }
    prev_lines_count = line_Sprite;
}

//----------------------------------------------------------------------------------------------------------
static void Draw_circular_scale()
{
    int koeff_line = 10;
    /* Рисуем круглую шкалу серым цветом и символы сторон света белым*/
    for (int i = 0; i < 36; i++)
    {
        // unsigned short color2 = TFT_DARKGREY;
        if (i % 3 == 0)
        {
            backsprite.drawWedgeLine(fx[i * koeff_line], fy[i * koeff_line], px[i * koeff_line], py[i * koeff_line], 1, 1, TFT_DARKGREY);
            backsprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
            if (i == 0)
            {
                backsprite.drawString("N", lx[i * koeff_line] + 1, ly[i * koeff_line], TFT_DARKGREY);
            }
            if (i == 9)
            {
                backsprite.drawString("E", lx[i * koeff_line], ly[i * koeff_line], TFT_DARKGREY);
            }
            if (i == 18)
            {
                backsprite.drawString("S", lx[i * koeff_line], ly[i * koeff_line], TFT_DARKGREY);
            }
            if (i == 27)
            {
                backsprite.drawString("W", lx[i * koeff_line], ly[i * koeff_line], TFT_DARKGREY);
            }
        }
        else
        {
            backsprite.drawWedgeLine(fx[i * koeff_line], fy[i * koeff_line], px1[i * koeff_line], py1[i * koeff_line], 1, 1, TFT_DARKGREY);
        }
    }
    esp_task_wdt_reset();

    /*Рисуем кольцо диапазона, которому соответствует подпись шкалы*/
    backsprite.drawCircle(kRadarPivotX, kRadarPivotY, kRadarRingRadiusPx, TFT_DARKGREY);

}


#define MESSAGE_X      2           // Позиция сообщения
#define MESSAGE_Y      20
#define MESSAGE_W      480          // Ширина области сообщения
#define MESSAGE_H      40           // Высота области сообщения
#define MESSAGE_COLOR  TFT_YELLOW    // Цвет текста
#define BG_COLOR       TFT_BLACK    // Цвет фона для "стирания" сообщения

//const char* message = "Привет, мир!";
unsigned long lastToggle = 0;
const unsigned long showPeriod = 4000;     // 4 секунды показа
const unsigned long hidePeriod = 1000;     // 1 секунда скрытия
bool isMessageVisible = false;
bool isMessageDeleted = false;



// Скрыть сообщение (именно закрасить область)
void hideMessage()
{
    if (rows_Message.created())
    {
        rows_Message.deleteSprite();
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Вывод направления движения
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


namespace {
struct RadarScaleDef
{
    int32_t ringMeters;
    int32_t maxVisibleMeters;
    const char* label;
};

static const RadarScaleDef kRadarScaleDefs[] = {
    {32000, 64000, "32 km"},
    {16000, 32000, "16 km"},
    { 8000, 16000,  "8 km"},
    { 4000,  8000,  "4 km"},
    { 2000,  4000,  "2 km"},
    { 1000,  2000,  "1 km"},
    {  500,  1000, "500 m"},
    {  250,   500, "250 m"}
};

static constexpr uint8_t kRadarScaleCount = sizeof(kRadarScaleDefs) / sizeof(kRadarScaleDefs[0]);

static float deg2radRadar(float value)
{
    return value * 0.01745329251994329577f;
}

static float rad2degRadar(float value)
{
    return value * 57.295779513082320876f;
}

static float radarBearingDeg(float lat1, float lon1, float lat2, float lon2)
{
    const float phi1 = deg2radRadar(lat1);
    const float phi2 = deg2radRadar(lat2);
    const float dLon = deg2radRadar(lon2 - lon1);
    const float y = sinf(dLon) * cosf(phi2);
    const float x = cosf(phi1) * sinf(phi2) - sinf(phi1) * cosf(phi2) * cosf(dLon);
    float brng = rad2degRadar(atan2f(y, x));
    if (brng < 0.0f)
    {
        brng += 360.0f;
    }
    return brng;
}

static float radarDistanceMeters(float lat1, float lon1, float lat2, float lon2)
{
    const float dLat = deg2radRadar(lat2 - lat1);
    const float dLon = deg2radRadar(lon2 - lon1);
    const float sinLat = sinf(dLat * 0.5f);
    const float sinLon = sinf(dLon * 0.5f);
    const float a = sinLat * sinLat + cosf(deg2radRadar(lat1)) * cosf(deg2radRadar(lat2)) * sinLon * sinLon;
    const float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return 6371000.0f * c;
}

static bool updateTrafficPolarFromCoords(ufo_t& traffic)
{
    if (traffic.latitude == 0.0f || traffic.longitude == 0.0f ||
        ThisAircraft.local_latitude == 0.0f || ThisAircraft.local_longitude == 0.0f)
    {
        return false;
    }

    traffic.distance = radarDistanceMeters(ThisAircraft.local_latitude, ThisAircraft.local_longitude,
                                           traffic.latitude, traffic.longitude);
    traffic.bearing = radarBearingDeg(ThisAircraft.local_latitude, ThisAircraft.local_longitude,
                                      traffic.latitude, traffic.longitude);
    return true;
}

static uint8_t clampRadarScaleIndex(uint8_t index)
{
    return (index < kRadarScaleCount) ? index : (kRadarScaleCount - 1U);
}

static uint8_t autoRadarScaleIndex(uint32_t nearestMeters)
{
    // Автомасштаб выбираем так, чтобы ближайшая цель попадала внутрь основного кольца радара.
    if (nearestMeters > 16000U) return 0; // ring 32 km
    if (nearestMeters >  8000U) return 1; // ring 16 km
    if (nearestMeters >  4000U) return 2; // ring 8 km
    if (nearestMeters >  2000U) return 3; // ring 4 km
    if (nearestMeters >  1000U) return 4; // ring 2 km
    if (nearestMeters >   500U) return 5; // ring 1 km
    if (nearestMeters >   250U) return 6; // ring 500 m
    return 7;                              // ring 250 m
}

static uint8_t effectiveRadarScaleIndex(uint32_t nearestMeters)
{
    if (settings != nullptr && settings->radar_range_mode != 0)
    {
        return 0;
    }
    if (set_view_range != 0)
    {
        return clampRadarScaleIndex((uint8_t)(set_view_range - 1U));
    }
    return autoRadarScaleIndex(nearestMeters);
}

static float radarDistanceToPixels(const RadarScaleDef& scale, float distanceMeters)
{
    const float clampedDistance = constrain(distanceMeters, 0.0f, (float)scale.maxVisibleMeters);
    const float ringDistance = (float)scale.ringMeters;
    const float ringRadiusPx = (float)kRadarRingRadiusPx;
    const float outerRadiusPx = (float)kRadarTargetOuterRadiusPx;

    if (clampedDistance <= ringDistance || scale.maxVisibleMeters <= scale.ringMeters)
    {
        return (ringDistance > 0.0f) ? (clampedDistance * ringRadiusPx / ringDistance) : 0.0f;
    }

    const float outerDistance = (float)(scale.maxVisibleMeters - scale.ringMeters);
    const float outerRadiusSpan = outerRadiusPx - ringRadiusPx;
    return ringRadiusPx + ((clampedDistance - ringDistance) * outerRadiusSpan / outerDistance);
}


static bool radarProjectTarget(int16_t bearingDeg, float distanceMeters, const RadarScaleDef& scale, int16_t& outX, int16_t& outY)
{
    if (distanceMeters < 0.0f || distanceMeters > (float)scale.maxVisibleMeters)
    {
        outX = 0;
        outY = 0;
        return false;
    }

    const float radialPixels = radarDistanceToPixels(scale, distanceMeters);
    outX = constrain((int32_t)lroundf(radialPixels * sinf(radians(bearingDeg))), -32768, 32767);
    outY = constrain((int32_t)lroundf(radialPixels * cosf(radians(bearingDeg))), -32768, 32767);
    return true;
}

struct RadarOverlayLayout
{
    int16_t labelRelX;
    int16_t labelRelY;
    bool labelOnLeft;
};

struct RadarPlacedSymbol
{
    int16_t absX;
    int16_t absY;
    bool valid;
};

struct RadarPlacedLabel
{
    int16_t absX;
    int16_t absY;
    int16_t width;
    int16_t height;
    bool valid;
};

enum RadarOverlaySide : uint8_t
{
    RADAR_OVERLAY_BELOW = 0,
    RADAR_OVERLAY_LEFT  = 1,
    RADAR_OVERLAY_ABOVE = 2,
    RADAR_OVERLAY_RIGHT = 3
};

static constexpr int16_t kRadarLabelSpriteWidth = 55;
static constexpr int16_t kRadarLabelSpriteHeight = 15;
static constexpr int16_t kRadarTrendArrowGlyphWidth = 7;
static constexpr int16_t kRadarTrendArrowGlyphHeight = 9;
static constexpr int16_t kRadarTrendArrowGlyphGapPx = 1;
static constexpr int16_t kRadarTargetDotRadiusPx = 6;
static constexpr int16_t kRadarLabelGapPx = 0;
static constexpr int16_t kRadarOverlayScreenMinX = 0;
static constexpr int16_t kRadarOverlayScreenMaxX = 480;
static constexpr int16_t kRadarOverlayScreenMinY = 0;
static constexpr int16_t kRadarOverlayScreenMaxY = 320;
static constexpr int16_t kRadarOverlayBlockWidth = kRadarLabelSpriteWidth;
static constexpr int16_t kRadarOverlayBlockHeight = kRadarLabelSpriteHeight;
static constexpr int16_t kRadarTargetCollisionRadiusPx = 18;
static constexpr int16_t kRadarLabelCollisionPaddingPx = 2;
static constexpr uint8_t kRadarOverlayGapVariantsCount = 6;
static const int16_t kRadarOverlayGapVariants[kRadarOverlayGapVariantsCount] = {0, 4, 8, 12, 16, 20};

static int16_t radarClampOverlayAbsX(int16_t value, int16_t width)
{
    const int16_t maxX = (int16_t)(kRadarOverlayScreenMaxX - width);
    return constrain(value, kRadarOverlayScreenMinX, maxX);
}

static int16_t radarClampOverlayAbsY(int16_t value, int16_t height)
{
    const int16_t maxY = (int16_t)(kRadarOverlayScreenMaxY - height);
    return constrain(value, kRadarOverlayScreenMinY, maxY);
}

static int16_t normalizeRadarAngle360(int16_t angleDeg)
{
    int16_t value = angleDeg % 360;
    if (value < 0)
    {
        value += 360;
    }
    return value;
}

static RadarOverlaySide radarOverlaySideFromCourse(int16_t courseDeg)
{
    const int16_t normalized = normalizeRadarAngle360(courseDeg);

    if (normalized < 45 || normalized >= 315)
    {
        return RADAR_OVERLAY_BELOW;
    }
    if (normalized < 135)
    {
        return RADAR_OVERLAY_LEFT;
    }
    if (normalized < 225)
    {
        return RADAR_OVERLAY_ABOVE;
    }
    return RADAR_OVERLAY_RIGHT;
}

static RadarOverlaySide radarRotateSideCW(RadarOverlaySide side)
{
    return (RadarOverlaySide)((side + 1U) & 0x03U);
}

static RadarOverlaySide radarRotateSideCCW(RadarOverlaySide side)
{
    return (RadarOverlaySide)((side + 3U) & 0x03U);
}

static RadarOverlaySide radarOppositeSide(RadarOverlaySide side)
{
    return (RadarOverlaySide)((side + 2U) & 0x03U);
}

static bool radarRectsOverlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                              int16_t bx, int16_t by, int16_t bw, int16_t bh,
                              int16_t padding)
{
    return !((ax + aw + padding) <= bx || (bx + bw + padding) <= ax ||
             (ay + ah + padding) <= by || (by + bh + padding) <= ay);
}

static bool radarRectOverlapsPlacedLabels(int16_t x, int16_t y, int16_t w, int16_t h,
                                          const RadarPlacedLabel* placedLabels, int placedLabelCount)
{
    for (int idx = 0; idx < placedLabelCount; ++idx)
    {
        if (!placedLabels[idx].valid)
        {
            continue;
        }
        if (radarRectsOverlap(x, y, w, h,
                              placedLabels[idx].absX, placedLabels[idx].absY,
                              placedLabels[idx].width, placedLabels[idx].height,
                              kRadarLabelCollisionPaddingPx))
        {
            return true;
        }
    }
    return false;
}

static bool radarRectOverlapsPlacedSymbols(int16_t x, int16_t y, int16_t w, int16_t h,
                                           const RadarPlacedSymbol* placedSymbols, int placedSymbolCount)
{
    for (int idx = 0; idx < placedSymbolCount; ++idx)
    {
        if (!placedSymbols[idx].valid)
        {
            continue;
        }
        if (radarRectsOverlap(x, y, w, h,
                              placedSymbols[idx].absX - kRadarTargetCollisionRadiusPx,
                              placedSymbols[idx].absY - kRadarTargetCollisionRadiusPx,
                              kRadarTargetCollisionRadiusPx * 2,
                              kRadarTargetCollisionRadiusPx * 2,
                              0))
        {
            return true;
        }
    }
    return false;
}

static void radarClampAbsPointToVisibleCircle(float& absX, float& absY)
{
    const float dx = absX - (float)kRadarPivotX;
    const float dy = absY - (float)kRadarPivotY;
    const float distance = sqrtf(dx * dx + dy * dy);
    if (distance > (float)kRadarTargetOuterRadiusPx && distance > 0.001f)
    {
        const float scale = (float)kRadarTargetOuterRadiusPx / distance;
        absX = (float)kRadarPivotX + dx * scale;
        absY = (float)kRadarPivotY + dy * scale;
    }
}

static void radarResolveTargetCollision(int16_t& targetRelX, int16_t& targetRelY,
                                        const RadarPlacedSymbol* placedSymbols, int placedSymbolCount,
                                        const RadarPlacedLabel* placedLabels, int placedLabelCount)
{
    float absX = (float)(kRadarPivotX + targetRelX);
    float absY = (float)(kRadarPivotY - targetRelY);

    for (int iteration = 0; iteration < 10; ++iteration)
    {
        bool moved = false;

        for (int idx = 0; idx < placedSymbolCount; ++idx)
        {
            if (!placedSymbols[idx].valid)
            {
                continue;
            }

            float dx = absX - (float)placedSymbols[idx].absX;
            float dy = absY - (float)placedSymbols[idx].absY;
            float distance = sqrtf(dx * dx + dy * dy);

            if (distance < 0.01f)
            {
                dx = (idx & 1) ? 1.0f : -1.0f;
                dy = (idx & 2) ? 1.0f : -1.0f;
                distance = 1.4142135f;
            }

            if (distance < (float)(kRadarTargetCollisionRadiusPx * 2))
            {
                const float push = ((float)(kRadarTargetCollisionRadiusPx * 2) - distance) + 1.0f;
                absX += (dx / distance) * push;
                absY += (dy / distance) * push;
                moved = true;
            }
        }

        for (int idx = 0; idx < placedLabelCount; ++idx)
        {
            if (!placedLabels[idx].valid)
            {
                continue;
            }

            const int16_t left = placedLabels[idx].absX - kRadarTargetCollisionRadiusPx;
            const int16_t top = placedLabels[idx].absY - kRadarTargetCollisionRadiusPx;
            const int16_t width = placedLabels[idx].width + (kRadarTargetCollisionRadiusPx * 2);
            const int16_t height = placedLabels[idx].height + (kRadarTargetCollisionRadiusPx * 2);

            if (absX >= left && absX <= (left + width) && absY >= top && absY <= (top + height))
            {
                const float rectCenterX = (float)placedLabels[idx].absX + (placedLabels[idx].width * 0.5f);
                const float rectCenterY = (float)placedLabels[idx].absY + (placedLabels[idx].height * 0.5f);
                float dx = absX - rectCenterX;
                float dy = absY - rectCenterY;
                float distance = sqrtf(dx * dx + dy * dy);
                if (distance < 0.01f)
                {
                    dx = (idx & 1) ? 1.0f : -1.0f;
                    dy = -1.0f;
                    distance = 1.4142135f;
                }
                absX += (dx / distance) * 6.0f;
                absY += (dy / distance) * 6.0f;
                moved = true;
            }
        }

        radarClampAbsPointToVisibleCircle(absX, absY);

        if (!moved)
        {
            break;
        }
    }

    targetRelX = (int16_t)lroundf(absX - (float)kRadarPivotX);
    targetRelY = (int16_t)lroundf((float)kRadarPivotY - absY);
}

static void radarPlaceOverlayBlockForSide(RadarOverlaySide side, int16_t targetAbsX, int16_t targetAbsY, int16_t gapPx,
                                          int16_t& blockAbsX, int16_t& blockAbsY, bool& labelOnLeft)
{
    blockAbsX = targetAbsX - (kRadarOverlayBlockWidth / 2);
    blockAbsY = targetAbsY - (kRadarOverlayBlockHeight / 2);
    labelOnLeft = true;

    switch (side)
    {
    case RADAR_OVERLAY_LEFT:
        blockAbsX = targetAbsX - kRadarTargetDotRadiusPx - gapPx - kRadarOverlayBlockWidth;
        blockAbsY = targetAbsY - (kRadarOverlayBlockHeight / 2);
        labelOnLeft = false;
        break;
    case RADAR_OVERLAY_RIGHT:
        blockAbsX = targetAbsX + kRadarTargetDotRadiusPx + gapPx;
        blockAbsY = targetAbsY - (kRadarOverlayBlockHeight / 2);
        labelOnLeft = true;
        break;
    case RADAR_OVERLAY_ABOVE:
        blockAbsX = targetAbsX - (kRadarOverlayBlockWidth / 2);
        blockAbsY = targetAbsY - kRadarTargetDotRadiusPx - gapPx - kRadarOverlayBlockHeight;
        labelOnLeft = true;
        break;
    case RADAR_OVERLAY_BELOW:
    default:
        blockAbsX = targetAbsX - (kRadarOverlayBlockWidth / 2);
        blockAbsY = targetAbsY + kRadarTargetDotRadiusPx + gapPx;
        labelOnLeft = true;
        break;
    }
}

static RadarOverlayLayout radarBuildOverlayLayout(int16_t targetRelX, int16_t targetRelY, int16_t courseDeg,
                                                  const RadarPlacedSymbol* placedSymbols, int placedSymbolCount,
                                                  const RadarPlacedLabel* placedLabels, int placedLabelCount)
{
    RadarOverlayLayout bestLayout = {};
    const int16_t targetAbsX = kRadarPivotX + targetRelX;
    const int16_t targetAbsY = kRadarPivotY - targetRelY;
    const RadarOverlaySide preferredSide = radarOverlaySideFromCourse(courseDeg);
    const RadarOverlaySide sideOrder[4] = {
        preferredSide,
        radarRotateSideCW(preferredSide),
        radarRotateSideCCW(preferredSide),
        radarOppositeSide(preferredSide)
    };

    int bestPenalty = 32767;

    for (int sideIndex = 0; sideIndex < 4; ++sideIndex)
    {
        for (int gapIndex = 0; gapIndex < (int)kRadarOverlayGapVariantsCount; ++gapIndex)
        {
            int16_t blockAbsX = 0;
            int16_t blockAbsY = 0;
            bool labelOnLeft = true;
            radarPlaceOverlayBlockForSide(sideOrder[sideIndex], targetAbsX, targetAbsY,
                                          (int16_t)(kRadarLabelGapPx + kRadarOverlayGapVariants[gapIndex]),
                                          blockAbsX, blockAbsY, labelOnLeft);

            blockAbsX = radarClampOverlayAbsX(blockAbsX, kRadarOverlayBlockWidth);
            blockAbsY = radarClampOverlayAbsY(blockAbsY, kRadarOverlayBlockHeight);

            const bool overlapsLabels = radarRectOverlapsPlacedLabels(blockAbsX, blockAbsY,
                                                                      kRadarLabelSpriteWidth, kRadarLabelSpriteHeight,
                                                                      placedLabels, placedLabelCount);
            const bool overlapsSymbols = radarRectOverlapsPlacedSymbols(blockAbsX, blockAbsY,
                                                                        kRadarLabelSpriteWidth, kRadarLabelSpriteHeight,
                                                                        placedSymbols, placedSymbolCount);

            const int penalty = (overlapsLabels ? 1000 : 0) +
                                (overlapsSymbols ? 200 : 0) +
                                (sideIndex * 10) + gapIndex;

            RadarOverlayLayout candidate = {};
            candidate.labelRelX = blockAbsX - kRadarPivotX;
            candidate.labelRelY = kRadarPivotY - blockAbsY;
            candidate.labelOnLeft = labelOnLeft;

            if (!overlapsLabels && !overlapsSymbols)
            {
                return candidate;
            }

            if (penalty < bestPenalty)
            {
                bestPenalty = penalty;
                bestLayout = candidate;
            }
        }
    }

    return bestLayout;
}

static void radarDrawTrendArrowGlyph(TFT_eSprite& sprite, int16_t x, int16_t y, bool isClimbing, uint16_t color)
{
    const int16_t clampedX = constrain((int32_t)x, 0, (int32_t)(kRadarLabelSpriteWidth - kRadarTrendArrowGlyphWidth));
    const int16_t clampedY = constrain((int32_t)y, 0, (int32_t)(kRadarLabelSpriteHeight - kRadarTrendArrowGlyphHeight));
    const int16_t centerX = clampedX + (kRadarTrendArrowGlyphWidth / 2);

    if (isClimbing)
    {
        sprite.fillTriangle(centerX, clampedY,
                            clampedX, clampedY + 3,
                            clampedX + kRadarTrendArrowGlyphWidth - 1, clampedY + 3,
                            color);
        sprite.drawLine(centerX, clampedY + 3, centerX, clampedY + kRadarTrendArrowGlyphHeight - 1, color);
    }
    else
    {
        sprite.drawLine(centerX, clampedY, centerX, clampedY + kRadarTrendArrowGlyphHeight - 4, color);
        sprite.fillTriangle(centerX, clampedY + kRadarTrendArrowGlyphHeight - 1,
                            clampedX, clampedY + kRadarTrendArrowGlyphHeight - 4,
                            clampedX + kRadarTrendArrowGlyphWidth - 1, clampedY + kRadarTrendArrowGlyphHeight - 4,
                            color);
    }
}

static void radarDrawOwnshipSymbol(TFT_eSprite& sprite)
{
    const int x = kRadarOwnshipCenterX;
    const int y = kRadarOwnshipCenterY;

    sprite.drawLine(x, y - 10, x, y + 8, TFT_DARKGREY);          // фюзеляж
    sprite.drawLine(x - 9, y - 2, x + 8, y - 2, TFT_DARKGREY);   // передние крылья
    sprite.drawLine(x - 12, y - 1, x + 11, y - 1, TFT_DARKGREY);
    sprite.drawLine(x - 5, y + 8, x + 5, y + 8, TFT_DARKGREY);   // задние крылья
}
}

int alien_count()
{
    int count = 0;

    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container[i].addr)
        {
            count++;
        }
    }

    return count;
}

bool coordinates_waiting()
{
    bool coord = false;

    return coord;
}



//==============================================================================

static void drawLanInfoBlock()
{
    if (settings && settings->lan_state_view == 0)
    {
        return;
    }

    uint32_t lanTxPackets = 0;
    uint32_t lanRxPackets = 0;
    uint32_t lanUdpTxPackets = 0;
    uint32_t lanUdpRxPackets = 0;
    uint32_t lanTcpTxPackets = 0;
    uint32_t lanTcpRxPackets = 0;
    LAN_getPacketCounters(lanTxPackets, lanRxPackets, lanUdpTxPackets, lanUdpRxPackets, lanTcpTxPackets, lanTcpRxPackets);

    back.setTextFont(2);
    back.setTextSize(1);

    const int xRight = 478;
    const int xLeftInfo = 2;
    const int y0 = 259;
    const int dy = 15;

    if (settings && settings->rssi_view == VIEW_RSSI_ON)
    {
        back.setTextDatum(TL_DATUM);
        back.setTextColor(TFT_DARKGREY, TFT_BLACK);
        const uint32_t rfFreqHz = RF_GetCurrentFrequencyHz();
        if (rfFreqHz > 0UL)
        {
            const uint32_t mhz = rfFreqHz / 1000000UL;
            const uint32_t khz = (rfFreqHz % 1000000UL) / 1000UL;
            char freqLine[32];
            snprintf(freqLine, sizeof(freqLine), "RF %lu.%03lu", (unsigned long)mhz, (unsigned long)khz);
            back.drawString(String(freqLine), xLeftInfo, y0);
        }
        else
        {
            back.drawString(String("RF ---"), xLeftInfo, y0);
        }
    }

    back.setTextDatum(TR_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
    back.drawString(LAN_addressModeStr(), xRight, y0);
    back.drawString(String("IP ") + LAN_localIPStr(), xRight, y0 + dy);

    const int y1 = y0 + dy * 2;
    const String lanLabel = String("LAN ");
    const String udpLabel = String("  UDP ");
    const String lanStateWord = LAN_linkUp() ? String("On") : String("Off");
    const String udpStateWord = LAN_udpWorking() ? String("On") : String("Off");

    int lanLabelW = back.textWidth(lanLabel, 2);
    int lanStateW = back.textWidth(lanStateWord, 2);
    int udpLabelW = back.textWidth(udpLabel, 2);
    int udpStateW = back.textWidth(udpStateWord, 2);
    int totalW = lanLabelW + lanStateW + udpLabelW + udpStateW;
    int xText = xRight - totalW;

    back.setTextDatum(TL_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
    back.drawString(lanLabel, xText, y1);
    xText += lanLabelW;
    back.setTextColor(LAN_linkUp() ? TFT_GREEN : TFT_RED, TFT_BLACK);
    back.drawString(lanStateWord, xText, y1);
    xText += lanStateW;
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
    back.drawString(udpLabel, xText, y1);
    xText += udpLabelW;
    back.setTextColor(LAN_udpWorking() ? TFT_GREEN : TFT_RED, TFT_BLACK);
    back.drawString(udpStateWord, xText, y1);

    back.setTextDatum(TR_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
    back.drawString(String("Tx ") + String(lanTxPackets) + String("  Rx ") + String(lanRxPackets), xRight, y0 + dy * 3);
}

void Display_loop()
{
    char buf[16];
    uint32_t disp_value;

    uint16_t tbw;
    uint16_t tbh;

    if (!TFT_display_frontpage)
    {
        FlyRfSpiGuard spiGuard(100);
        if (spiGuard)
        {
            tft.fillScreen(TFT_NAVY);
        }
        back.fillSprite(backColor);                    // Закрасим поле 
        backsprite.fillSprite(backColor);              // 
        angle = 0;                                     // (360 - (int)ThisAircraft.course) % 360;

        /*Выполняем поворот по азимуту*/
        backsprite.pushRotated(&back, angle, TFT_BLACK);
        /***************    TFT_шкала дистанции    *******************/
        {
            FlyRfSpiGuard spiGuard(150);
            if (spiGuard)
            {
                back.pushSprite(0, 0);
            }
        }
        TFT_display_frontpage = true;
    }
    else
    {
        /* TFT_display_frontpage  Основная программа отображения воздушной обстановки*/

        //-------------------- Блок работы с кнопкой  -----------------------------------------
        //******************** выполнение действий кнопок ******************************

        uint8_t new_buttton = takeDisplayButtonEvent(); 

        if (settings != nullptr && settings->radar_range_mode != 0)
        {
            set_view_range = 0;
        }
        else
        {
            if (set_view_range != 0)
            {
                static uint32_t button_tmr = millis();
                if (millis() - button_tmr > BUTTON_OFF_DELAY)
                {
                    button_tmr = millis();
                    set_view_range = 0;
                }
            }

            if (new_buttton != 0)
            {
                switch (new_buttton)
                {
                case 1:
                    set_view_range++;
                    if (set_view_range > kRadarScaleCount)
                    {
                        set_view_range = 0;
                    }
                    break;
                case 2:
                case 3:
                    set_view_range = 0;
                    break;
                default:
                    break;
                }

                new_buttton = 0;
            }
        }



        // execution_state_SOS_button();

         //============================== Основной блок вывода воздушной обстановки на экран ========================================================
        static uint32_t tmr = millis();

        /* Проверяем наличие новой информации */
        if (millis() - tmr > DATA_MEASURE_THRESHOLD)
        {
            tmr = millis();
            int Air_txt_x = 41;              // Расположение текста в формуляре стороннего самолета 

            back.fillSprite(backColor);                   // Закрасим поле 
            backsprite.fillSprite(TFT_BLACK);             // 
            backsprite.setPivot(240, 240);                // Назначаем центр вращения спрайта воздушной обстановки

            /* =================  Сначала зафиксируем положение нашего самолета =================================*/

            //=========================== Сглаживаем основные показатели скорости, высоты  и курса ==================================

            /* =========== Фильтр скорости нашего самолета. ================== */

            bool array_countMax_speed = false;
            int sum_speed = 0;
            uint8_t array_count_speed = 0;
            uint8_t array_size_speed = 20;
            int dimension_array_speed[20];

            dimension_array_speed[array_count_speed] = (int)ThisAircraft.speed;
            array_count_speed++;
            int val_speed = 0;
            if (array_count_speed > array_size_speed)               // проверка заполнения массива первичными данными об величине скорости
            {
                array_count_speed = 0;
                array_countMax_speed = true;                       //Разрешить выдавать данные об величине скорости
            }

            sum_speed = 0;                                         //

            if (array_countMax_speed)                              // формируем данные об величине скорости
            {
                for (int i = 0; i < array_size_speed; i++)
                {
                    sum_speed += dimension_array_speed[i];
                }
                val_speed = sum_speed / array_size_speed;
            }
            else
            {
                for (int i = 0; i < array_count_speed; i++)       //формируем первичные (заполняем массив) данные об величине скорости
                {
                    sum_speed += dimension_array_speed[array_count_speed - 1];
                }
                val_speed = sum_speed / array_count_speed;
            }
            sum_speed = 0;
            // thisAircraft_speed_tmr = val_speed;                  // Данные по скорости нашего самолета после фильтра
            thisAircraft_speed_tmr = 0;                            // Стационарный вариант. Данные по скорости нашего самолета после фильтра

            /*========== Фильтр курса нашего самолета ================*/

            bool array_countMax_course = false;
            int sum_course = 0;
            uint8_t array_count_course = 0;
            uint8_t array_size_course = 15;
            int dimension_array_course[15];

            dimension_array_course[array_count_course] = (int)ThisAircraft.course;
            array_count_course++;
            int val_course = 0;
            if (array_count_course > array_size_course)             // проверка заполнения массива первичными данными об величине курса
            {
                array_count_course = 0;
                array_countMax_course = true;                       //Разрешить выдавать данные об величине курса
            }

            sum_course = 0;                                         //

            if (array_countMax_course)                              // формируем данные об величине курса
            {
                for (int i = 0; i < array_size_course; i++)
                {
                    sum_course += dimension_array_course[i];
                }
                val_course = sum_course / array_size_course;
            }
            else
            {
                for (int i = 0; i < array_count_course; i++)       //формируем первичные (заполняем массив) данные об величине курса
                {
                    sum_course += dimension_array_course[array_count_course - 1];
                }
                val_course = sum_course / array_count_course;
            }

            sum_course = 0;
            angle = (360 - val_course) % 360;                   // Данные по курсу нашего самолета после фильтра


            /* При малой скорости нашего самолета фиксируем курс нашего самолета */

            if (thisAircraft_speed_tmr >= 0 && thisAircraft_speed_tmr < 5)
            {
                angle = angle_old;
            }
            else
            {
                angle_old = angle;
            }

            angle = 0; // Стационарный вариант.

            //======================= Фильтр высоты нашего самолета ==========================
            bool array_countMax_altitude = false;
            int sum_altitude = 0;
            uint8_t array_count_altitude = 0;
            uint8_t array_size_altitude = 20;
            int dimension_array_altitude[20];

            dimension_array_altitude[array_count_altitude] = (int)ThisAircraft.altitude;
            array_count_altitude++;
            int val_altitude = 0;
            if (array_count_altitude > array_size_altitude)               // проверка заполнения массива первичными данными об величине курса
            {
                array_count_altitude = 0;
                array_countMax_altitude = true;                           //Разрешить выдавать данные об величине курса
            }

            sum_altitude = 0;                                             //

            if (array_countMax_altitude)                                  // формируем данные об величине курса
            {
                for (int i = 0; i < array_size_altitude; i++)
                {
                    sum_altitude += dimension_array_altitude[i];
                }
                val_altitude = sum_altitude / array_size_altitude;
            }
            else
            {
                for (int i = 0; i < array_count_altitude; i++)       //формируем первичные (заполняем массив) данные об величине курса
                {
                    sum_altitude += dimension_array_altitude[array_count_altitude - 1];
                }
                val_altitude = sum_altitude / array_count_altitude;
            }

            sum_altitude = 0;

            thisAircraft_altitude_tmr = val_altitude;  // Данные по высоте нашего самолета после фильтра 

            //===================================== Закончили ввод данных нашего самолета ==============================================


            /* ======================= Определим наличие сторонних самолетов и отобразим их на экране ==============================*/

            view_alien_count = alien_count();        // Смотрим сколько сторонних самолетов зафиксировано в базе
            {
                const uint8_t defaultRangeIndex = effectiveRadarScaleIndex((uint32_t)kRadarScaleDefs[0].maxVisibleMeters);
                const RadarScaleDef& defaultScale = kRadarScaleDefs[defaultRangeIndex];
                divider = defaultScale.ringMeters;
                divider_num = defaultRangeIndex + 1;
            }

            if (view_alien_count >= 1)
            {
                esp_task_wdt_reset();


                displayAllPlanes();                  // Вывести текстовую информацию по самолетам на дисплей


           //=====================================================================================
                /* Определяем какие пакеты приняты в текущем периоде*/
                /* Определяем минимальную дистанцию между нашим и сторонни самолетом и курс стороннего самолета*/
                uint32_t min_distance = 0xFFFFFFFFUL;

                for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
                {
                    if (Container[i].addr && (now() - Container[i].timestamp) <= TFT_EXPIRATION_TIME)  // Если есть самолет в базе и подошло время обновления данных
                    {
                        isTeam_all[i] = true;                                 // Сторонние самолеты определены и зарегтстрированы в базе

                        if (updateTrafficPolarFromCoords(Container[i]))
                        {
                            const uint32_t trafficDistance = (uint32_t)lroundf(Container[i].distance);
                            if (trafficDistance < min_distance)
                            {
                                min_distance = trafficDistance;
                                index_nearest_aircraft = i;
                                alient_course0 = Container[i].course;
                                alient_speed0 = Container[i].speed;
                            }
                        }
                    }
                    else
                    {
                        isTeam_all[i] = false;
                    }

                    esp_task_wdt_reset();
                }

                if (min_distance == 0xFFFFFFFFUL)
                {
                    min_distance = (uint32_t)kRadarScaleDefs[0].maxVisibleMeters;
                }

                const uint8_t activeRangeIndex = effectiveRadarScaleIndex(min_distance);
                const RadarScaleDef& activeScale = kRadarScaleDefs[activeRangeIndex];
                divider = activeScale.ringMeters;
                divider_num = activeRangeIndex + 1;


                // Установки определения уровней предупреждения. Параметры задаются со смартфона и записываются в EEPROM

                // settings->alarm_attention;     // Внимание. Параметр - расстояние 
                // settings->alarm_warning;       // Предупреждение. Параметр - расстояние 
                // settings->alarm_danger;        // Тревога. Параметр - расстояние        
                // settings->alarm_height;        // Тревога по высоте. Параметр - высота 


                //===================================================================================

                bool rssi_off = false;
                RadarPlacedSymbol placedSymbols[MAX_TRACKING_OBJECTS] = {};
                RadarPlacedLabel placedLabels[MAX_TRACKING_OBJECTS] = {};
                int placedSymbolCount = 0;
                int placedLabelCount = 0;

                for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
                {
                    if (Container[i].addr)  // Если есть данные стороннего самолета
                    {
                        esp_task_wdt_reset();
                        //=============================== фильтруем показания скорости стороннего самолета ==========================================

                            /* Сначала заполняем массив фильтра данными по скорости */

                        alien_speed_filtre[i][alien_speed_array_count[i]] = (int)Container[i].speed;
                        int alien_val_speed = 0;

                        if (alien_speed_array_countMax[i])                        // формируем данные о величине скорости
                        {
                            for (int k = 0; k < speed_array_size; k++)
                            {
                                alien_speed_sum[i] += alien_speed_filtre[i][k];
                            }
                            alien_val_speed = alien_speed_sum[i] / speed_array_size;
                            alien_speed_sum[i] = 0;
                        }

                        alien_speed_array_count[i]++;
                        if (alien_speed_array_count[i] > speed_array_size - 1)   // проверка заполнения массива первичными данными о скорости
                        {
                            alien_speed_array_count[i] = 0;
                            alien_speed_array_countMax[i] = true;                //Разрешить выдавать данные о величине скорости
                        }
                        alien_speed_tmr[i] = alien_val_speed;                    // Скорость стороннего самолета после фильтра

                        //================================== Фильтр высоты стороннего самолета =============================================

                        if ((int)Container[i].altitude != 0)
                        {
                            alien_altitude_filtre[i][alien_altitude_array_count[i]] = (int)Container[i].altitude;
                        }

                        int alien_val_altitude = 0;

                        if (alien_altitude_array_countMax[i])                            // формируем данные о высоте
                        {
                            for (int k = 0; k < altitude_array_size; k++)
                            {
                                alien_altitude_sum[i] += alien_altitude_filtre[i][k];
                            }
                            alien_val_altitude = alien_altitude_sum[i] / altitude_array_size;
                            alien_altitude_sum[i] = 0;
                        }

                        alien_altitude_array_count[i]++;

                        if (alien_altitude_array_count[i] > altitude_array_size - 1)         // проверка заполнения массива первичными данными высоте
                        {
                            alien_altitude_array_count[i] = 0;
                            alien_altitude_array_countMax[i] = true;                         // Флаг готовности данные о высоте стороннего самолета
                        }

                        if (alien_val_altitude != 0)
                        {
                            alien_altitude_actual[i] = alien_val_altitude;                   // Данные высоты стороннего самолета после фильтра            
                        }

                        /* Устанавливаем ограничение высоты стороннего самолета с применением гистерезиса */
                        int diff_altitude = 10; // Не реагировать если изменение меньше

                        if ((alien_altitude_actual[i] - alien_altitude_old[i] > diff_altitude) || alien_altitude_old[i] - alien_altitude_actual[i] > diff_altitude)
                        {

                            alien_altitude_old[i] = alien_altitude_actual[i];                 // Окончательные данные по высоте стороннего самолета с учетом гистерезиса.
                            alien_altitude_hysteresis[i] = alien_altitude_actual[i];          // Окончательные данные по высоте стороннего самолета с учетом гистерезиса.
                        }
                        //=================================================================================================

                        /* Определяем разность высот между нашим и сторонним самолетом. Нужно для вывода текста в формуляр */
                        int VerticalSet = 0;  // абсолютная величина без учета знака

                        if (alien_altitude_actual[i] != 0)
                        {
                            int RelativeVertical = alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr;   // Определяем разность высот

                            if (RelativeVertical >= 0)
                            {
                                VerticalSet = alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr;        // Данные разности высот со знаком +
                            }
                            else if (RelativeVertical < 0)
                            {
                                VerticalSet = thisAircraft_altitude_tmr - alien_altitude_hysteresis[i];        // Данные разности высот со знаком -
                            }
                        }


                        /* Вычисляем разность высот между нашим самолетом и сторонним. Данне со знаком + или -*/
                        height_difference[i] = alien_altitude_hysteresis[i] - thisAircraft_altitude_tmr;

                        //============================  Определение направления стрелок подъем или снижение ==========================================================
                        /*
                        Напоминание: alien_altitude_array_countMax  это флаг готовности данные о высоте стороннего самолета
                        */

                        if (alien_altitude_array_countMax[i] && alien_altitude_hysteresis[i] != 0)  //
                        {
                            if (millis() - tmr_array[i] > 100 + (DATA_MEASURE_THRESHOLD * 4))  //Исключить мигание стрелки вверх/вниз. Небходимо немного времени для изменения высоты самолета
                            {
                                tmr_array[i] = millis();

                                /* Напоминание
                                    old_alien_altitude_arrow -  Предыдущая высота стороннего самолета для отображения стрелок выше/ниже.
                                    alien_altitude_hysteresis - Обработанная высота стороннего самолета
                                */

                                if (alien_altitude_hysteresis[i] > old_alien_altitude_arrow[i] && old_alien_altitude_arrow[i] != 0) // При старых нулевых значениях не имеет смысла сравнивать
                                {
                                    arrow_up_down[i] = 1;
                                }
                                else if (alien_altitude_hysteresis[i] < old_alien_altitude_arrow[i] && old_alien_altitude_arrow[i] != 0) // При старых нулевых значениях не имеет смысла сравнивать
                                {
                                    arrow_up_down[i] = 2;
                                }
                                else
                                {
                                    arrow_up_down[i] = 0;
                                }
                                old_alien_altitude_arrow[i] = alien_altitude_hysteresis[i];
                            }
                        }
                        else
                        {
                            arrow_up_down[i] = 0;
                        }

                        //========================== Если координаты стороннего самолета определены ====================================

                        if (Container[i].latitude != 0.0 && Container[i].longitude != 0.0)
                        {
                            esp_task_wdt_reset();
                            // --------------------------------------------------------------------------------
                                /* При малой скорости смотрим в центр экрана на наш самолет. Это означает что самолет не летит (на земле) */
                            if (alien_speed_tmr[i] >= 0 && alien_speed_tmr[i] < 10)
                            {
                                Container[i].course = (180 + (int)Container[i].bearing) % 360;
                            }

                            /* курс стороннего самолета с учетом поворота экрана */
                            alient_course[i] = (angle + (int)Container[i].course) % 360;

                            /*Расчет координат сторонних самолетов на неподвижном экране с поправкой на вращение*/
                            new_angle[i] = (angle + (int)Container[i].bearing) % 360;

                            /*
                                Положение цели на TFT рассчитываем в два участка:
                                1) от центра до кольца:   0 .. ringMeters  -> 0 .. радиус кольца
                                2) от кольца до края: ringMeters .. maxVisibleMeters -> радиус кольца .. край экрана
                                Так подпись шкалы соответствует именно нарисованному кольцу.
                            */
                            if (!radarProjectTarget(new_angle[i], Container[i].distance, activeScale, new_x, new_y))
                            {
                                continue;
                            }

                            /* Разносим цели и формуляры в пространстве, чтобы уменьшить наложение.
                               Смещение выполняем только в отображении на TFT, координаты трафика не меняем. */
                            radarResolveTargetCollision(new_x, new_y, placedSymbols, placedSymbolCount, placedLabels, placedLabelCount);
                            Container_alien_X[i] = new_x;  // Сохранить экранные координаты стороннего самолета
                            Container_alien_Y[i] = new_y;

                            /* Формуляр всегда располагаем на стороне, противоположной направлению движения,
                               а при наложении ищем ближайшее свободное место вокруг цели. */
                            const RadarOverlayLayout overlayLayout = radarBuildOverlayLayout(new_x, new_y, alient_course[i],
                                                                                              placedSymbols, placedSymbolCount,
                                                                                              placedLabels, placedLabelCount);
                            Air_txt_left[i] = overlayLayout.labelOnLeft;
                            Container_logbook_X[i] = overlayLayout.labelRelX;
                            Container_logbook_Y[i] = overlayLayout.labelRelY;

                            esp_task_wdt_reset();

                            /* Определяем цвет текстов предупреждения об опастности */


                            if (min_distance >= settings->alarm_attention)   // Чужой самолет очень далеко
                            {
                                little_air_color[i] = TFT_WHITE;    // Цвет для вывода изображения самолетика 
                            }
                            else if (min_distance <= settings->alarm_attention && min_distance > settings->alarm_warning) // Чужой самолет на расстоянии предупреждения
                            {
                                if (VerticalSet > settings->alarm_height)   //  Чужой самолет выше расстояния опасности
                                {
                                    little_air_color[i] = TFT_WHITE;
                                }
                                else
                                {
                                    //  Чужой самолет на расстоянии предупреждения
                                    little_air_color[i] = TFT_YELLOW;
                                }
                            }
                            else if (min_distance <= settings->alarm_warning && min_distance > settings->alarm_danger)   // Чужой самолет на расстоянии предупреждения
                            {
                                if (VerticalSet > settings->alarm_height)                                                // Чужой самолет выше расстояния предупреждения
                                {
                                    little_air_color[i] = TFT_WHITE;
                                }
                                else if (VerticalSet <= settings->alarm_height)
                                {
                                    // Чужой самолет на расстоянии предупреждения
                                    little_air_color[i] = TFT_ORANGE;
                                }
                            }
                            else if (min_distance <= settings->alarm_danger && VerticalSet <= settings->alarm_height)  // Чужой самолет на близком расстоянии и по высоте опасен
                            {
                                little_air_color[i] = TFT_RED;
                            }
                            esp_task_wdt_reset();

                            /* Настраиваем вывод текста в формуляр скорость подвижного стороннего самолета */
                            Air_txt_Sprite[i]->fillSprite(TFT_BLACK);                        // Закрасим поле сообщений
                            Air_txt_Sprite[i]->setTextColor(little_air_color[i], backColor);    // Цвет формуляра
                            Air_txt_Sprite[i]->setTextDatum(TL_DATUM);                           // Выводим строку слева направо
                            Air_txt_Sprite[i]->loadFont(NotoSansBold15);                         // Шрифт формуляра

                            /*===============  Этот фрагмент для вывода самолета в движении*/
                            /* Запись параметров высоты в формуляр + символ стрелки в том же спрайте */
                            if (Container[i].addr)
                            {
                                const int height_tmp = (int)round(height_difference[i] / 10);
                                const String heightText = (height_tmp > 0) ? ("+" + String(height_tmp)) : String(height_tmp);
                                Air_txt_Sprite[i]->drawString(heightText, 1, 1, 0);

                                if (arrow_up_down[i] == 1 || arrow_up_down[i] == 2)
                                {
                                    int16_t arrowX = (int16_t)Air_txt_Sprite[i]->textWidth(heightText) + kRadarTrendArrowGlyphGapPx;
                                    if (arrowX > (kRadarLabelSpriteWidth - kRadarTrendArrowGlyphWidth))
                                    {
                                        arrowX = kRadarLabelSpriteWidth - kRadarTrendArrowGlyphWidth;
                                    }
                                    radarDrawTrendArrowGlyph(*Air_txt_Sprite[i], arrowX, 3, arrow_up_down[i] == 1, little_air_color[i]);
                                }
                            }

                            /* Записать скорость в формуляр  движущегося самолета*/
                            alien_speed_view[i] = 50 - ((int)Container[i].speed / 60 * 3);  // Расстояние стороннего самолета для вывода на дисплей

                            if (alien_speed_view[i] > 40)
                            {
                                alien_speed_view[i] = 40;
                            }

                            /*Рисуем маленький самолетик в виде закрашенного круга */
                            airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле самолетика
                            airplane[i]->fillCircle(50, 50, 6, little_air_color[i]),                     // fillCircle //drawCircle
                                airplane[i]->drawLine(49, 45, 49, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                            airplane[i]->drawLine(50, 45, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                            airplane[i]->drawLine(51, 45, 51, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                            area_airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле 

                            if (placedSymbolCount < MAX_TRACKING_OBJECTS)
                            {
                                placedSymbols[placedSymbolCount].absX = (int16_t)(kRadarPivotX + Container_alien_X[i]);
                                placedSymbols[placedSymbolCount].absY = (int16_t)(kRadarPivotY - Container_alien_Y[i]);
                                placedSymbols[placedSymbolCount].valid = true;
                                ++placedSymbolCount;
                            }
                            if (placedLabelCount < MAX_TRACKING_OBJECTS)
                            {
                                placedLabels[placedLabelCount].absX = (int16_t)(kRadarPivotX + Container_logbook_X[i]);
                                placedLabels[placedLabelCount].absY = (int16_t)(kRadarPivotY - Container_logbook_Y[i]);
                                placedLabels[placedLabelCount].width = kRadarLabelSpriteWidth;
                                placedLabels[placedLabelCount].height = kRadarLabelSpriteHeight;
                                placedLabels[placedLabelCount].valid = true;
                                ++placedLabelCount;
                            }

                            airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK); // 
                            area_airplane[i]->pushToSprite(&back, kRadarPivotX + Container_alien_X[i] - 50, kRadarPivotY - Container_alien_Y[i] - 50, TFT_BLACK);
                            Air_txt_Sprite[i]->pushToSprite(&back, kRadarPivotX + Container_logbook_X[i], kRadarPivotY - Container_logbook_Y[i], TFT_BLACK);

                            esp_task_wdt_reset();

                        } //Закочить обработку данных самолетов с известными координатами

                        //============================= Конец обработки данных самолетов самолетов  с известными координатами ==========================
                    }

                    /* Отобразить уровень сигнала приема информации от конкретного самолета*/
                    if (settings->rssi_view == VIEW_RSSI_ON) // Если вывод уровня сигнала разрешен в WEB интерфейсе
                    {
                        back.setTextDatum(0);
                        back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                        int rssi_y = 274;

                        if (Container[i].rssi_LoRa < 0 && Container[i].signal_source == 1)
                        {
                            back.drawString("      ", 1, 210);
                            back.drawString(String("RSSI ") + String(Container[i].rssi_LoRa) + String(" db"), 2, rssi_y);
                            rssi_off = true;
                        }

                        if (!rssi_off)
                        {
                            back.drawString("                    ", 2, rssi_y);
                        }
                    }
                }
            }
            else
            {
                int rssi_y = 274;
            }
            //============================== Формируем неподвижное базовое изображение на экране =========================================== 

                /* настройки вывода времени на экран*/
            time_info.setFreeFont(&FreeSerif12pt7b);
            time_info.fillSprite(TFT_BLACK);
            time_info.setTextDatum(CC_DATUM);
            time_info.setTextColor(TFT_GREEN, backColor);

            Draw_circular_scale();
            esp_task_wdt_reset();

            /*Выполняем поворот нашего самолета по азимуту*/
            backsprite.pushRotated(&back, angle, TFT_BLACK);

            esp_task_wdt_reset();
            if (divider <= 32767)
            {
                back.loadFont(NotoSansBold15);
                back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                back.setTextDatum(TC_DATUM);
                const int data_KM_x = kRadarRangeLabelX;
                const int data_KM_y = kRadarRangeLabelY;

                const uint8_t dividerIndex = clampRadarScaleIndex((uint8_t)(divider_num - 1));
                const RadarScaleDef& scaleForText = kRadarScaleDefs[dividerIndex];
                back.drawString(scaleForText.label, data_KM_x - 3, data_KM_y);

                if (set_view_range != 0 || (settings != nullptr && settings->radar_range_mode != 0))
                {
                    back.drawRect(data_KM_x - 30, data_KM_y - 1, 56, 17, TFT_RED);
                }
            }

            if (settings->view_test_coord == VIEW_COORD_ON) // Стационарный вариант
            {
                back.setTextDatum(0);
                back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                back.drawString("Lat: " + String(ThisAircraft.local_latitude, 5), 1, 287);
                back.drawString("Lon: " + String(ThisAircraft.local_longitude, 5), 1, 303);
            }

            esp_task_wdt_reset();

            /*Формируем картинку нашего самолета*/
            radarDrawOwnshipSymbol(back);
            //============================== Конец формирования неподвижного базовоо изображения на экране =========================================== 

            esp_task_wdt_reset();

            /*рисуем все спрайты*/

            drawLanInfoBlock();
            {
                FlyRfSpiGuard spiGuard(150);
                if (spiGuard)
                {
                    back.pushSprite(0, 0);
                }
            }
        }
    }
}




//void Display_loop()
//{
//}
