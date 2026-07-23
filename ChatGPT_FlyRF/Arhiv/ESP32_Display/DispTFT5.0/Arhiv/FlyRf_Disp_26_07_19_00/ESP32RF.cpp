/*
  Модуль ESP32RF.cpp
  Назначение:
  - Графический интерфейс и радарная визуализация на TFT-дисплее ESP32S3.

  Основные задачи модуля:
  - Инициализировать TFT, спрайты и экранные зоны.
  - Отображать наш самолет, сторонние цели, формуляры, векторы скорости и предупреждения.
  - Обслуживать разные режимы показа, масштабирования и поворота радара.
  - Выводить служебную информацию: время, статус GNSS, LAN, LoRa и другие показатели.
*/

#include <Arduino.h>
#include "ESP32RF.h"
#include "HardwareConfig.h"
#include "ProjectHardware.h"
#if PROJECT_USE_GL050001C0_40
#include "GL050001C0_40_Display.h"
#endif
#include "TrafficDB.h"
#include "EEPROMRF.h"
#include "RS485Display.h"
#include "DisplayRemote.h"
#include "WiFiRF.h"
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include "Container.h"
#include "DeviceInfo.h"
#include "System.h"
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Внешний дисплей не содержит базового SPI/RF-модуля.
// Легкий совместимый guard нужен, чтобы экранный код компилировался автономно.
#ifndef FLYRF_DISPLAY_SPI_GUARD_FALLBACK
#define FLYRF_DISPLAY_SPI_GUARD_FALLBACK
class FlyRfSpiGuard
{
public:
    explicit FlyRfSpiGuard(uint32_t) {}
    explicit operator bool() const { return true; }
};
#endif

// RAW LoRa есть только в базовом модуле. На внешнем дисплее RF-пакеты не принимаются напрямую.
#ifndef FLARM_RFM95_MAX_PACKET
#define FLARM_RFM95_MAX_PACKET 64U
#endif
struct FlarmRawPacket
{
    uint8_t  data[FLARM_RFM95_MAX_PACKET];
    size_t   length = 0;
    int16_t  rssi = 0;
    uint32_t receivedAt = 0;
    bool     valid = false;
};
static bool RF_GetLastRawPacket(FlarmRawPacket&) { return false; }

#include "fonts/NotoSansMonoSCB20.h"
#include "fonts/NotoSansBold15.h"

#include "fonts/zTimesNRItalic28.h"
#include "fonts/zTimesNR48.h"
#include "fonts/zCalibri36.h"
#include "fonts/zTimesNR18.h"
#include "fonts/zTimesNR24.h"
#include "fonts/zTimesNR28.h"

#define AA_FONT_CALI zCalibri36
#define AA_FONT_TIME28I zTimesNRItalic28
#define AA_FONT_TIME48 zTimesNR48
#define AA_FONT_TIME18 zTimesNR18
#define AA_FONT_TIME24 zTimesNR24
#define AA_FONT_TIME28 zTimesNR28

#define WIDTH  480
#define HEIGHT 480

#define INFO_HEIGHT 17 

// create a new sprite
TFT_eSPI tft = TFT_eSPI();                          // Главный объект TFT-дисплея, через который выполняется инициализация и базовая отрисовка.

TFT_eSprite back = TFT_eSprite(&tft);               // Главный полноэкранный буфер фона. В него собирается кадр перед выводом на дисплей.
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Буфер вращающегося поля радара: окружности, риски, отметки и направляющие.
TFT_eSprite rows_Message = TFT_eSprite(&tft);       // Спрайт текстового сообщения от трекера, которое показывается в верхней части TFT.
TFT_eSprite location_Message = TFT_eSprite(&tft);   // Дополнительный спрайт служебных надписей и координатных блоков.
TFT_eSprite time_info = TFT_eSprite(&tft);          // Спрайт блока времени и сопутствующей верхней информации.
TFT_eSprite base_connection = TFT_eSprite(&tft);    // Спрайт блока статуса связи, LoRa, LAN и других нижних индикаторов.
TFT_eSprite info_header = TFT_eSprite(&tft);        // Спрайт заголовка текстовой таблицы сторонних бортов.

TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS] = { nullptr };  // Ранее отдельные спрайты формуляров; оставлены только как заглушки
TFT_eSprite* airplane[MAX_TRACKING_OBJECTS] = { nullptr };        // Ранее отдельные спрайты символов целей; отрисовка теперь прямая в общий буфер
TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS] = { nullptr };   // Ранее промежуточные спрайты вращения; больше не используются
TFT_eSprite power1 = TFT_eSprite(&tft);                           // Спрайт отображения заряда аккумулятора 
TFT_eSprite voltage1 = TFT_eSprite(&tft);                         // Спрайт отображения напряжения аккумулятора 
TFT_eSprite current1 = TFT_eSprite(&tft);                         // Спрайт отображения ток потребления 
TFT_eSprite* airplane_alient_info[MAX_TRACKING_OBJECTS] = { nullptr }; // Массив строковых спрайтов для табличного вывода данных по каждому борту.

static bool g_displayReady = false;                                    // Флаг успешной инициализации дисплея и основных графических ресурсов.
static Adafruit_INA219 g_ina219;
static bool g_ina219Ready = false;
static uint32_t g_ina219LastProbeMs = 0;
static uint32_t g_ina219LastReadMs = 0;
static bool g_powerTelemetryValid = false;
static float g_powerVoltageV = 0.0f;
static float g_powerCurrentMa = 0.0f;
static uint8_t g_powerBatteryPercent = 0;


// Нормализует курс дисплея к диапазону 0..359 градусов.
// Используется перед поворотом радарного поля и вычислением ориентации объектов.

static int normalizeDisplayHeading360(int headingDeg)
{
    headingDeg %= 360;
    if (headingDeg < 0)
    {
        headingDeg += 360;
    }
    return headingDeg;
}

// Возвращает угол поворота радарного поля с учетом текущего курса отображения.
// Нужен для режима Heading Up и согласованного вращения компаса и целей.

static int currentRadarRotationAngleDeg()
{
    const int headingDeg = normalizeDisplayHeading360((int)lroundf(SystemDisplayCourseDeg()));
    return (360 - headingDeg) % 360;
}

// Выбирает цвет предупреждения для цели по дальности и разнице высот.
// На выходе цвет метки: белый, желтый, оранжевый или красный.

static uint16_t radarAlertColorForTarget(uint32_t distanceMeters, int verticalDistanceMeters, bool verticalValid)
{
    if (settings == nullptr)
    {
        return TFT_WHITE;
    }

    if (distanceMeters > (uint32_t)settings->alarm_attention)
    {
        return TFT_WHITE;
    }

    if (!verticalValid || verticalDistanceMeters > settings->alarm_height)
    {
        return TFT_WHITE;
    }

    if (distanceMeters <= (uint32_t)settings->alarm_danger)
    {
        return TFT_RED;
    }
    if (distanceMeters <= (uint32_t)settings->alarm_warning)
    {
        return TFT_ORANGE;
    }
    return TFT_YELLOW;
}

// Преобразует цвет предупреждения в числовой уровень тревоги.
// Используется при логике сортировки и отображения опасных целей.

static int8_t radarAlertLevelForColor(uint16_t color)
{
    if (color == TFT_RED)    return 3;
    if (color == TFT_ORANGE) return 2;
    if (color == TFT_YELLOW) return 1;
    return 0;
}

// Создает спрайт и проверяет, что память выделилась успешно.
// Вариант для уже существующего объекта спрайта.

static bool createSpriteChecked(TFT_eSprite& sprite, int16_t width, int16_t height, uint8_t colorDepth, const char* name)
{
    sprite.setColorDepth(colorDepth);
    sprite.createSprite(width, height);
    if (!sprite.created())
    {
        return false;
    }
    return true;
}

// Создает спрайт и проверяет, что память выделилась успешно.
// Вариант для указателя на спрайт из массива.

static bool createSpriteChecked(TFT_eSprite* sprite, int16_t width, int16_t height, uint8_t colorDepth, const char* name)
{
    if (sprite == nullptr)
    {
        //Serial.printf("[DISPLAY] sprite object alloc failed: %s\r\n", name ? name : "sprite");
        return false;
    }
    sprite->setColorDepth(colorDepth);
    sprite->createSprite(width, height);
    if (!sprite->created())
    {
      //  Serial.printf("[DISPLAY] createSprite failed: %s (%d x %d, depth=%u)\r\n",     name ? name : "sprite", (int)width, (int)height, (unsigned)colorDepth);
        return false;
    }
    return true;
}


// Форматирует знаковое целое число в строку без принудительного плюса.
// Применяется при подготовке текстовых полей формуляров и телеметрии.

static void formatSignedValue(char* out, size_t outSize, int value)
{
    if (out == nullptr || outSize == 0) return;
    snprintf(out, outSize, "%d", value);
}

// Форматирует число в строку с символом плюс для положительных значений.
// Удобно для вертикальной скорости и разницы высот.

static void formatSignedWithPlus(char* out, size_t outSize, int value)
{
    if (out == nullptr || outSize == 0) return;
    if (value > 0) snprintf(out, outSize, "+%d", value);
    else snprintf(out, outSize, "%d", value);
}

// Форматирует число с плавающей точкой в текст без ведущих пробелов.
// Используется при выводе координат и прочих дробных параметров.

//------------------------------------------------------------------------------
static void formatFloatValue(char* out, size_t outSize, float value, uint8_t digits)
{
    if (out == nullptr || outSize == 0) return;
    dtostrf((double)value, 0, digits, out);
    while (*out == ' ')
    {
        ++out;
    }
}

// Группы массивов ниже хранят расчетные и экранные параметры по каждому стороннему борту.
// Индекс массива соответствует индексу цели в Container[].
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
static constexpr int16_t kRadarPivotX = 240;                     // Центр радарного поля по оси X.
static constexpr int16_t kRadarPivotY = 240;                     // Центр радарного поля по оси Y.
static constexpr int16_t kRadarCompassOuterRadiusPx = 233;       // Внешний радиус круговой шкалы компаса.
static constexpr int16_t kRadarCompassMajorInnerRadiusPx = 224;  // Внутренний радиус длинных рисок компаса.
static constexpr int16_t kRadarCompassMinorInnerRadiusPx = 229;  // Внутренний радиус коротких рисок компаса.
static constexpr int16_t kRadarCompassLabelRadiusPx = 232;       // Радиус расположения подписей сторон света.
static constexpr int16_t kRadarCompassNeedleRadiusPx = 202;      // Радиус линии направления/компасной стрелки.
static constexpr int16_t kRadarRingRadiusPx = 120;               // Базовый радиус внутреннего кольца дальности.
static constexpr int16_t kRadarTargetOuterRadiusPx = 224;        // Максимальный радиус размещения целей внутри радара.
static constexpr int16_t kRadarOwnshipCenterX = kRadarPivotX;    // Координата X символа нашего самолета.
static constexpr int16_t kRadarOwnshipCenterY = kRadarPivotY;    // Координата Y символа нашего самолета.
static constexpr int16_t kRadarRangeLabelX = kRadarPivotX + 6;   // Координата X подписи текущего масштаба.
static constexpr int16_t kRadarRangeLabelY = 303;                // Координата Y подписи текущего масштаба.

int nc = 0;                                       // Служебный счетчик/индекс для вспомогательных расчетов шкал.

float fx[360];                                    // Координаты X внешних точек окружности/шкалы радара по каждому градусу.
float fy[360];                                    // Координаты Y внешних точек окружности/шкалы радара по каждому градусу.
float px[360];                                    // Координаты X внутренних точек основных рисок шкалы.
float py[360];                                    // Координаты Y внутренних точек основных рисок шкалы.
float px1[360];                                   // Координаты X вспомогательных внутренних точек шкалы.
float py1[360];                                   // Координаты Y вспомогательных внутренних точек шкалы.
float lx[360];                                    // Координаты X подписей/меток по окружности.
float ly[360];                                    // Координаты Y подписей/меток по окружности.
float nx[360];                                    // Координаты X для направляющих/стрелок шкалы.
float ny[360];                                    // Координаты Y для направляющих/стрелок шкалы.

double rad = 0.01745;                             // Коэффициент перевода градусов в радианы для быстрой графики.
int angle = 0;                                    // Текущий рабочий угол для промежуточных вычислений.
int angle_old = 0;                                // Предыдущий угол для расчета изменений и перерисовки.


int thisAircraft_altitude_tmr = 0;       // Текущая отображаемая высота нашего самолета.
int thisAircraft_speed_tmr = 0;          // Текущая отображаемая скорость нашего самолета.
int thisAircraft_course_tmr = 0;         // Текущий отображаемый курс нашего самолета.
//
//#ifndef DISPLAY_BUTTON_PIN
//#define DISPLAY_BUTTON_PIN 45
//#endif
//
//#ifndef DISPLAY_BUTTON_ACTIVE_LEVEL
//#define DISPLAY_BUTTON_ACTIVE_LEVEL LOW
//#endif
//
//#ifndef SOS_INPUT_PIN
//#define SOS_INPUT_PIN 42
//#endif
//
//#ifndef SOS_INPUT_ACTIVE_LEVEL
//#define SOS_INPUT_ACTIVE_LEVEL HIGH
//#endif
//
//#ifndef DISPLAY_BUTTON_DEBOUNCE_MS
//#define DISPLAY_BUTTON_DEBOUNCE_MS 30UL
//#endif
//
//#ifndef DISPLAY_BUTTON_DOUBLE_MS
//#define DISPLAY_BUTTON_DOUBLE_MS 350UL
//#endif
//
//#ifndef DISPLAY_BUTTON_LONG_MS
//#define DISPLAY_BUTTON_LONG_MS 900UL
//#endif

static volatile uint8_t g_pendingDisplayButton = 0;  // Очередь из одного события кнопки дисплея: 1-клик, 2-двойной, 3-длинный.
static TaskHandle_t g_displayButtonTask = nullptr;   // Дескриптор FreeRTOS-задачи опроса кнопки дисплея.
static uint32_t g_manualRangeActivatedMs = 0U;       // Время активации ручного диапазона просмотра.
static uint8_t g_lastEffectiveRadarScaleIndex = 0U;  // Последний реально использованный индекс диапазона радара.
static uint8_t g_lastAutoRadarScaleIndex = 0U;       // Последний автоматически выбранный индекс диапазона радара.
static bool g_sosInputActive = false;                // Текущее состояние входа SOS GPIO42.
static bool g_sosOverlayVisible = false;             // Текущая фаза мигания SOS: true = надпись должна отображаться.
static uint32_t g_sosBlinkCycleStartMs = 0U;         // Начало цикла мигания SOS 4 сек. видно / 1 сек. скрыто.

#ifndef SOS_BLINK_ON_MS
#define SOS_BLINK_ON_MS 3000UL
#endif

#ifndef SOS_BLINK_OFF_MS
#define SOS_BLINK_OFF_MS 500UL
#endif


// Забирает следующее событие кнопки TFT/RS485-дисплея.
// Приоритет у событий, пришедших по RS485, затем используется локальная очередь кнопки.
//------------------------------------------------------------------------------
// Назначение функции: Забирает следующее событие кнопки дисплея.
// Сначала проверяет очередь кнопок внешнего дисплея RS485, затем локальную кнопку GPIO 45.
// Возвращает код события: одиночное, двойное или длинное нажатие. Если событий нет — возвращает 0.
// Локальные переменные:
//   rs485Event - код события, полученный от внешнего дисплея по RS485.
//   event     - код локального события, накопленного задачей опроса кнопки.
//------------------------------------------------------------------------------
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

// Помещает событие кнопки в однобайтную очередь.
// Событие затем обрабатывается в основном цикле экрана.
//------------------------------------------------------------------------------
// Назначение функции: Помещает новое событие кнопки в локальную очередь обработки.
// Используется задачей опроса кнопки, чтобы основной цикл Display_loop() позже разобрал нажатие.
// Локальные переменные:
// event - код события кнопки: одно нажатие, двойное или длинное.
//------------------------------------------------------------------------------
static void pushDisplayButtonEvent(uint8_t event)
{
    if (event == 0) return;
    g_pendingDisplayButton = event;
    RS485Display_setLocalButtonEvent(event);
}

// Задача FreeRTOS для опроса физической кнопки дисплея.
// Выполняет антидребезг, определяет одинарное, двойное и длинное нажатие.
//------------------------------------------------------------------------------
// Назначение функции: Задача FreeRTOS для опроса физической кнопки на GPIO 45.
// Выполняет антидребезг контактов, различает одиночное, двойное и длинное нажатия
// и передаёт распознанное событие в Display_loop() через pushDisplayButtonEvent().
// Локальные переменные:
//   stableState     - устойчивое состояние кнопки после антидребезга.
//   lastRawState    - последнее мгновенно считанное состояние входа GPIO.
//   pressActive     - признак того, что кнопка сейчас удерживается.
//   longSent        - признак того, что длинное нажатие уже сгенерировано.
//   waitSecondClick - ожидание второго нажатия для распознавания двойного клика.
//   rawChangedAt    - момент последнего изменения сырого сигнала кнопки.
//   pressedAt       - момент начала текущего нажатия.
//   firstReleaseAt  - момент отпускания кнопки после первого клика.
//   now             - текущее время millis() внутри цикла опроса.
//   rawState        - текущее мгновенно считанное состояние входа GPIO.
//------------------------------------------------------------------------------
static void button(void* param)
{
    (void)param;
    pinMode(DISPLAY_BUTTON_PIN, INPUT_PULLUP);

    bool stableState = (digitalRead(DISPLAY_BUTTON_PIN) == DISPLAY_BUTTON_ACTIVE_LEVEL);
    bool lastRawState = stableState;  // Последнее мгновенное состояние входа, ещё без антидребезга.
    bool pressActive = false;  // Признак того, что кнопка сейчас удерживается.
    bool longSent = false;  // Признак отправки события длинного нажатия для текущего удержания.
    bool waitSecondClick = false;  // Ожидание второго клика после первого короткого нажатия.
    uint32_t rawChangedAt = millis();  // Время последнего изменения сырого уровня на входе кнопки.
    uint32_t pressedAt = 0;  // Время начала текущего нажатия.
    uint32_t firstReleaseAt = 0;  // Время отпускания кнопки после первого короткого клика.

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

uint8_t index_nearest_aircraft = 0;      // Индекс ближайшего стороннего самолета в контейнере.
uint8_t  set_view_range = 0;             // Выбранный индекс масштаба/диапазона радара.
uint8_t  view_alien_count = 0;           // Количество сторонних самолетов, пригодных для показа на экране.

int16_t alient_course0 = 0;              // Курс ближайшего стороннего самолета.
int16_t alient_speed0 = 0;               // Скорость ближайшего стороннего самолета.
int8_t  txt_loc_speed = 87;              // Координата/смещение вывода текста скорости рядом с целью.

int32_t divider = 2000;                  // Делитель шкалы дальности, используется при переводе метров в пиксели.
int divider_num = 1;                     // Номер активного делителя/масштаба для радарной сетки.

int16_t new_x;                           // Временная X-координата при геометрических преобразованиях.
int16_t new_y;                           // Временная Y-координата при геометрических преобразованиях.

int16_t form_x = 0;                      // X-координата формуляра цели.
int16_t form_y = 0;                      // Y-координата формуляра цели.
int16_t form_arrow_x = 0;                // X-координата стрелки набора/снижения у формуляра.
int16_t form_arrow_y = 0;                // Y-координата стрелки набора/снижения у формуляра.

int test_curse = 0;                      // Служебный тестовый курс для отладочного режима.
float Aircraft_latitude_old1 = 0;        // Предыдущая широта нашего самолета для локальных вычислений отображения.
float Aircraft_longitude_old1 = 0;       // Предыдущая долгота нашего самолета для локальных вычислений отображения.
uint8_t  fix_tmp = false;                // Временный флаг наличия координат/фикса.

static bool TFT_display_frontpage = false; // Флаг активной стартовой/главной страницы TFT.


//====================================================================================



// Инициализирует TFT-дисплей, шрифты, спрайты, стартовый экран и задачу кнопки.
// Вызывается один раз при старте системы.
//------------------------------------------------------------------------------
// Назначение функции: Инициализирует служебную операцию модуля, подготавливает связанные объекты и включает работу соответствующего узла.
// Локальные переменные: Current_version — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// spriteInitOk — графический спрайт или буфер отрисовки, используемый для подготовки части экрана без мерцания; 
// a — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - tbw1: Параметр геометрии, координаты, размера или угла.
// - x_tft: Объект внешнего интерфейса, экрана, порта или канала связи.
// - y_tft: Объект внешнего интерфейса, экрана, порта или канала связи.
//------------------------------------------------------------------------------
void Display_setup()
{
    g_displayReady = false;
    // На внешнем дисплее SOS приходит только от базового модуля по RS485.

    if (g_displayButtonTask == nullptr)
    {
        xTaskCreatePinnedToCore(button, "Button", 2048, NULL, 2, &g_displayButtonTask, 0);
    }

#if PROJECT_USE_GL050001C0_40
    if (GL050001C0_40_setup())
    {
        GL050001C0_40_showStartup(DeviceInfo_programVersion());
        g_displayReady = true;
    }
    return;
#endif

    FlyRfSpiGuard spiGuard(500);
    if (spiGuard)
    {
        tft.begin();
        tft.setRotation(1);
        tft.fillScreen(TFT_NAVY);
    }
 
    tft.setTextColor(TFT_WHITE, TFT_BLACK);   // Set the font colour AND the background colour

    uint16_t tbw1;  
    uint16_t x_tft, y_tft;

    tft.loadFont(AA_FONT_CALI);               // Must load the font first

    tft.setCursor(130, 80);
    tft.println("FlyRF Display");
    tft.setCursor(170, 120);
    tft.println("DECIMA");

    tft.loadFont(AA_FONT_TIME28I);            // Must load the font first
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);  // Set the font colour AND the background colour
    tft.setCursor(140, 180);
    tft.println("ВКЛЮЧАЕТСЯ");
    tft.setCursor(155, 220);
    tft.println("ОЖИДАЙТЕ");

    tft.loadFont(AA_FONT_TIME18);            // Must load the font first
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

    bool spriteInitOk = true;  

    spriteInitOk &= createSpriteChecked(back, 480, 480, 8, "back");
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        // По целям больше не создаются отдельные спрайты.
        // Вся динамическая информация рисуется сразу в общий буфер back.
        Air_txt_Sprite[i] = nullptr;
        airplane[i] = nullptr;
        area_airplane[i] = nullptr;
        airplane_alient_info[i] = nullptr;

        alien_speed_array_countMax[i] = false;
        alien_speed_sum[i] = 0;
        alien_speed_array_count[i] = 0;

        alien_altitude_array_countMax[i] = false;
        alien_altitude_sum[i] = 0;
        alien_altitude_array_count[i] = 0;

        old_alien_altitude_arrow[i] = 0;                // Массив хранения предыдущих значений высоты, для формирования стрелок направления перемещения самолета вврх/вниз

        tmr_array[i] = millis();
        esp_task_wdt_reset();
    }

    spriteInitOk &= createSpriteChecked(backsprite, 480, 480, 8, "backsprite");
    if (backsprite.created())
    {
        backsprite.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
        backsprite.setSwapBytes(true);
        backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
        backsprite.setTextDatum(4);
        backsprite.setPivot(kRadarPivotX, kRadarPivotY); // Назначаем центр вращения спрайта воздушной обстановки
    }


    int a = 270;  
    for (int i = 0; i < 360; i++)
    {
        fx[i] = (kRadarCompassOuterRadiusPx * cos(rad * a)) + kRadarPivotX;           // внешняя точка крупной метки
        fy[i] = (kRadarCompassOuterRadiusPx * sin(rad * a)) + kRadarPivotY;
        px[i] = (kRadarCompassMajorInnerRadiusPx * cos(rad * a)) + kRadarPivotX;      // внутренняя точка крупной метки
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
    delay(3000);
    g_displayReady = true;
}

void Display_powerOff()
{
#if PROJECT_USE_GL050001C0_40
    GL050001C0_40_showPowerOff();
    return;
#endif

    FlyRfSpiGuard spiGuard(500);
    if (!spiGuard)
    {
        return;
    }

    tft.fillScreen(TFT_NAVY);
    tft.loadFont(AA_FONT_TIME28I);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(130, 130);
    tft.println("ОТКЛЮЧАЕТСЯ");
    tft.setCursor(153, 180);
    tft.println("ОЖИДАЙТЕ");
}


//========================= Функции вывода информации о сторонних объектах на экран ==================

static bool block_plane_display = false;  
int line_Sprite = 0;                      // текущая линия для вывода (строк без дыр)
static int prev_lines_count = 0;          // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.

// Вызывать эту функцию, чтобы полностью скрыть и отключить вывод авиа-треков и заголовка
// Полностью скрывает и удаляет текстовый список самолетов и его заголовок.
// Используется, когда верхняя область отдана под текстовое сообщение или другую служебную информацию.
//------------------------------------------------------------------------------
// Назначение функции: Очищает самолет, сбрасывает связанные флаги и возвращает модуль в исходное состояние для этого участка логики.

//------------------------------------------------------------------------------
void clearPlaneDisplay()
{
    if (!g_displayReady)
    {
        return;
    }

    // Отдельные строковые спрайты больше не используются.
    // Достаточно сбросить состояние, а на следующем цикле общий буфер будет перерисован заново.
    prev_lines_count = 0;
    block_plane_display = true;
}

// Чтобы разрешить обратно отрисовку авиа-треков:
// Разрешает повторную отрисовку табличной информации по самолетам.
// Вызывается после снятия блокировки верхней области дисплея.
//------------------------------------------------------------------------------
// Назначение функции: Включает самолет и подготавливает зависимые элементы к дальнейшей работе.

//------------------------------------------------------------------------------
void enablePlaneDisplay()
{
    block_plane_display = false;
}


// Разбивает сырой принятый LoRa/FLARM-пакет на две HEX-строки для показа на TFT.
// Применяется в сервисном режиме просмотра RAW-кадров.
//------------------------------------------------------------------------------
// Назначение функции: Формирует raw пакет hex lines в готовом виде для вывода, передачи или последующей обработки.
// Локальные переменные: char — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; fullHex — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; outPos — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; maxBytes — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; splitPos — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора.
// - hexDigits: Параметр геометрии, координаты, размера или угла.
//------------------------------------------------------------------------------
static void formatRawPacketHexLines(const FlarmRawPacket& packet, char* line1, size_t line1Size, char* line2, size_t line2Size)
{
    if (line1 && line1Size) line1[0] = 0;
    if (line2 && line2Size) line2[0] = 0;

    static const char hexDigits[] = "0123456789ABCDEF";  
    char fullHex[(FLARM_RFM95_MAX_PACKET * 2U) + 1U];
    size_t outPos = 0;  
    const size_t maxBytes = (packet.length > FLARM_RFM95_MAX_PACKET) ? FLARM_RFM95_MAX_PACKET : packet.length;

    for (size_t i = 0; i < maxBytes && (outPos + 2U) < sizeof(fullHex); ++i)
    {
        const uint8_t b = packet.data[i];  
        fullHex[outPos++] = hexDigits[(b >> 4) & 0x0F];
        fullHex[outPos++] = hexDigits[b & 0x0F];
    }
    fullHex[outPos] = 0;

    const size_t splitPos = 28U;  
    if (line1 && line1Size)
    {
        strncpy(line1, fullHex, line1Size - 1U);
        line1[line1Size - 1U] = 0;
        if (strlen(line1) > splitPos)
        {
            line1[splitPos] = 0;
        }
    }

    if (line2 && line2Size && outPos > splitPos)
    {
        strncpy(line2, fullHex + splitPos, line2Size - 1U);
        line2[line2Size - 1U] = 0;
    }
}

// Отрисовывает блок последнего принятого сырого LoRa-пакета на дисплее.
// Используется только в режиме вывода RAW RX.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает raw lo ra пакет block на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: boxX — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; boxY — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; boxW — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; boxH — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; headBuf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - headBuf: Буфер, текстовая строка или рабочее сообщение.
// - line1: Счетчик, индекс, позиция или номер элемента.
// - line2: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
static void drawRawLoRaPacketBlock()
{
    if (!g_displayReady || settings == nullptr || settings->display_set != INFO_DISPLAY_LORA_RAW)
    {
        return;
    }

    const int boxX = 2;  
    const int boxY = 22;  
    const int boxW = 476;  
    const int boxH = 38;  

    back.fillRoundRect(boxX, boxY, boxW, boxH, 6, TFT_BLACK);
    back.drawRoundRect(boxX, boxY, boxW, boxH, 6, TFT_DARKGREY);
    back.setTextDatum(TL_DATUM);
    back.setTextFont(2);
    back.setTextSize(1);
    back.setTextColor(TFT_YELLOW, TFT_BLACK);

    FlarmRawPacket packet = {};  
    if (!RF_GetLastRawPacket(packet) || !packet.valid || packet.length == 0)
    {
        back.drawString("LoRa RAW RX: ---", boxX + 6, boxY + 4, 2);
        back.setTextColor(TFT_DARKGREY, TFT_BLACK);
        back.drawString("Пакеты еще не приняты", boxX + 6, boxY + 20, 2);
        return;
    }

    char headBuf[48];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    const uint32_t ageMs = millis() - packet.receivedAt;
    snprintf(headBuf, sizeof(headBuf), "LoRa RAW RX RSSI %d LEN %u", packet.rssi, (unsigned)packet.length);
    back.drawString(headBuf, boxX + 6, boxY + 4, 2);

    char line1[40];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    char line2[40];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    formatRawPacketHexLines(packet, line1, sizeof(line1), line2, sizeof(line2));
    back.setTextColor(TFT_WHITE, TFT_BLACK);
    back.drawString(line1, boxX + 6, boxY + 20, 2);
    if (line2[0] != 0)
    {
        back.drawString(line2, boxX + 220, boxY + 20, 2);
    }
    if (ageMs > 9999UL)
    {
        back.setTextColor(TFT_DARKGREY, TFT_BLACK);
        back.drawString("old", boxX + boxW - 28, boxY + 4, 2);
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выводит all planes пользователю на экран или в текстовый блок интерфейса.
// Локальные переменные: has_data_to_display — логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные; 
// buf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст;
//  fltBuf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - buf: Буфер, текстовая строка или рабочее сообщение.
// - fltBuf: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
void displayAllPlanes() // Вывести текстовую информацию по самолетам на дисплей
{
    if (!g_displayReady)
        return;

    if (block_plane_display)
        return; // Блокировка активна — ничего не выводить!

    if (settings != nullptr && settings->display_set == INFO_DISPLAY_LORA_RAW)
        return; // Верхняя зона занята блоком сырых LoRa-пакетов.

    if (Tracker_hasActiveTextMessage())
        return; // Верхняя зона занята текстовым сообщением трекера.

    line_Sprite = 0;
    bool has_data_to_display = false;  

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

    back.setTextDatum(TL_DATUM);
    back.setTextColor(TFT_WHITE, backColor);
    back.setTextFont(2);
    back.setTextSize(1);

    if (has_data_to_display)
    {
        back.fillRect(0, 22, 480, 19, backColor);
        back.drawString("  ICAO  |SQUAWK|  FLIGHT  | ALT |SPEED|COURSE|LATITUDE|LONGITUDE| SIG", 0, 23, 2);
        back.drawLine(0, 40, 479, 40, TFT_WHITE);
    }

    char buf[32];     // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    char fltBuf[24];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.

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
                const int rowY = 41 + (line_Sprite * INFO_HEIGHT);
                back.fillRect(0, rowY, WIDTH, INFO_HEIGHT, backColor);
                back.setTextFont(2);
                back.setTextSize(1);

                snprintf(buf, sizeof(buf), "%06lX", (unsigned long)(Container[i].addr & 0xFFFFFFUL));
                back.drawString(buf, 3, rowY + 2, 2);

                if (Container[i].squawk != 0)
                {
                    snprintf(buf, sizeof(buf), "%d", Container[i].squawk);
                    back.drawString(buf, 65, rowY + 2, 2);
                }

                if (Container[i].callsign[0] != '\0')
                {
                    back.drawString(Container[i].callsign, 115, rowY + 2, 2);
                }

                if ((int)Container[i].altitude != 0)
                {
                    snprintf(buf, sizeof(buf), "%d", (int)Container[i].altitude);
                    back.drawString(buf, 178, rowY + 2, 2);
                }
                if ((int)Container[i].speed != 0)
                {
                    snprintf(buf, sizeof(buf), "%d", (int)Container[i].speed);
                    back.drawString(buf, 225, rowY + 2, 2);
                }
                if ((int)Container[i].course != 0)
                {
                    snprintf(buf, sizeof(buf), "%d", (int)Container[i].course);
                    back.drawString(buf, 270, rowY + 2, 2);
                }
                if (Container[i].latitude != 0)
                {
                    dtostrf((double)Container[i].latitude, 0, 5, fltBuf);
                    back.drawString(fltBuf, 313, rowY + 2, 2);
                }
                if (Container[i].longitude != 0)
                {
                    dtostrf((double)Container[i].longitude, 0, 5, fltBuf);
                    back.drawString(fltBuf, 382, rowY + 2, 2);
                }

                if (Container[i].signal_source == 1)
                {
                    if (Container[i].rssi_LoRa == 0)
                        back.drawString("--", 458, rowY + 2, 2);
                    else
                    {
                        snprintf(buf, sizeof(buf), "%d", Container[i].rssi_LoRa);
                        back.drawString(buf, 450, rowY + 2, 2);
                    }
                }
                else if (Container[i].signal_source == 2)
                {
                    if (Container[i].rssi_rp2040 == 0)
                        back.drawString("--", 458, rowY + 2, 2);
                    else
                    {
                        snprintf(buf, sizeof(buf), "%d", Container[i].rssi_rp2040);
                        back.drawString(buf, 450, rowY + 2, 2);
                    }
                }

                line_Sprite++;
            }
        }
    }

    prev_lines_count = line_Sprite;
}

//----------------------------------------------------------------------------------------------------------
// Рисует круговую шкалу компаса и кольца дальности радара.
// Подготавливает геометрию, которая лежит в основе фонового спрайта радара.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает circular масштаб радара на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: koeff_line — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
//------------------------------------------------------------------------------
static void Draw_circular_scale()
{
    int koeff_line = 10;  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
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


#define MESSAGE_X      2            // Позиция сообщения
#define MESSAGE_Y      20
#define MESSAGE_W      480          // Ширина области сообщения
#define MESSAGE_H      40           // Высота области сообщения
#define MESSAGE_COLOR  TFT_YELLOW   // Цвет текста
#define BG_COLOR       TFT_BLACK    // Цвет фона для "стирания" сообщения

unsigned long lastToggle = 0;             
const unsigned long showPeriod = 4000;     // 4 секунды показа
const unsigned long hidePeriod = 1000;     // 1 секунда скрытия
bool isMessageVisible = false;             
bool isMessageDeleted = false;         



// Скрыть сообщение (именно закрасить область)
// Скрывает текстовое сообщение трекера, закрашивая или удаляя спрайт сообщения.
// Мигание при этом может продолжаться, если флаг сообщения еще активен.
//------------------------------------------------------------------------------
// Назначение функции: Скрывает текстовое сообщение на экране, не затрагивая остальную логику модуля.

//------------------------------------------------------------------------------
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

// Переводит градусы в радианы для радарной геометрии.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `deg2radRadar` и обрабатывает deg2rad радар в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
static float deg2radRadar(float value)
{
    return value * 0.01745329251994329577f;
}

// Переводит радианы обратно в градусы.
static float rad2degRadar(float value)
{
    return value * 57.295779513082320876f;
}

// Вычисляет пеленг от нашего самолета до цели по координатам.
// Используется при позиционировании цели на радаре.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarBearingDeg` и обрабатывает радар bearing deg в контексте модуля ESP32RF.cpp.
// Локальные переменные: phi1 — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла;
//  phi2 — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла;
//  dLon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// y — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// x — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// brng — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

// Вычисляет расстояние между двумя точками в метрах.
// Нужен для выбора масштаба, предупреждений и размещения целей.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarDistanceMeters` и обрабатывает радар distance meters в контексте модуля ESP32RF.cpp.
// Локальные переменные: dLat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта;
// dLon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// sinLat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта;
// sinLon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта;
// a — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла;
// c — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

// Обновляет у цели полярные параметры: дальность, пеленг и сопутствующие расчеты.
// Возвращает false, если данных координат недостаточно.
//------------------------------------------------------------------------------
// Назначение функции: Обновляет запись о цели polar coords по новым входным данным и поддерживает актуальное состояние соответствующей подсистемы.

//------------------------------------------------------------------------------
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

// Ограничивает индекс масштаба радара допустимыми границами массива профилей.
static uint8_t clampRadarScaleIndex(uint8_t index)
{
    return (index < kRadarScaleCount) ? index : (kRadarScaleCount - 1U);
}

// Сбрасывает временный ручной диапазон и возвращает режим просмотра к исходному состоянию.
//------------------------------------------------------------------------------
// Назначение функции: Отменяет временный ручной диапазон радара и возвращает работу
// к штатному режиму выбора масштаба: автоматическому или ранее выбранному пользователем.
// Локальные переменные: функция работает только с глобальными переменными set_view_range
// и g_manualRangeActivatedMs, локальные данные не используются.
//------------------------------------------------------------------------------
static void clearManualRadarRangeOverride()
{
    set_view_range = 0;
    g_manualRangeActivatedMs = 0U;
}

void Display_clearManualRadarRangeOverride()
{
    clearManualRadarRangeOverride();
}

// Активирует временный ручной диапазон радара по указанному индексу шкалы.
//------------------------------------------------------------------------------
// Назначение функции: Включает временный ручной диапазон радара и запоминает момент
// его активации, чтобы через 5 минут можно было автоматически вернуть исходный режим.
// Локальные переменные:
// scaleIndex - индекс профиля шкалы радара в таблице kRadarScaleDefs.
//------------------------------------------------------------------------------
static void activateManualRadarScaleIndex(uint8_t scaleIndex)
{
    set_view_range = (uint8_t)(clampRadarScaleIndex(scaleIndex) + 1U);
    g_manualRangeActivatedMs = millis();
}

// Переключает диапазон просмотра на меньший и запускает 5-минутный таймер возврата.
//------------------------------------------------------------------------------
// Назначение функции: Уменьшает диапазон просмотра радара на один шаг при двойном нажатии
// кнопки дисплея и переводит радар во временный ручной режим на 5 минут.
// Локальные переменные:
// nextIndex - следующий индекс шкалы радара с меньшим диапазоном обзора.
//------------------------------------------------------------------------------
static void switchManualRadarToSmallerRange()
{
    // Если сейчас выбран автоматический режим, первое двойное нажатие
    // должно отталкиваться от реально рассчитанного автоматического масштаба,
    // а не от стартового/максимального диапазона.
    if (set_view_range == 0 && settings != nullptr && settings->radar_range_mode == 0)
    {
        activateManualRadarScaleIndex(0);
        return;
    }

    uint8_t nextIndex = g_lastEffectiveRadarScaleIndex;
    if (nextIndex + 1U < kRadarScaleCount)
    {
        ++nextIndex;
    }
    activateManualRadarScaleIndex(nextIndex);
}

// Подбирает автоматический масштаб радара по дистанции до ближайшей цели.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `autoRadarScaleIndex` и обрабатывает автоматический режим радар масштаб радара index в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
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

// Возвращает фактически используемый масштаб: ручной или автоматический.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `effectiveRadarScaleIndex` и обрабатывает effective радар масштаб радара index в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
static uint8_t effectiveRadarScaleIndex(uint32_t nearestMeters)
{
    if (set_view_range != 0)
    {
        return clampRadarScaleIndex((uint8_t)(set_view_range - 1U));
    }
    if (settings != nullptr && settings->radar_range_mode != 0)
    {
        return 0;
    }
    return autoRadarScaleIndex(nearestMeters);
}

// Переводит расстояние в метрах в радиальное смещение в пикселях для текущего масштаба.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarDistanceToPixels` и обрабатывает радар distance pixels в контексте модуля ESP32RF.cpp.
// Локальные переменные: clampedDistance — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// ringDistance — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// ringRadiusPx — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// outerRadiusPx — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта;
// outerDistance — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// outerRadiusSpan — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
//------------------------------------------------------------------------------
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


// Проецирует цель на экран радара по пеленгу и дальности.
// На выходе абсолютные координаты цели внутри радарного поля.
//------------------------------------------------------------------------------
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
static constexpr int16_t kRadarTrendArrowGlyphGapPx = 0;   
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

// Ограничивает X-координату формуляра/подписи, чтобы текст не выходил за пределы экрана.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarClampOverlayAbsX` и обрабатывает радар clamp overlay abs x в контексте модуля ESP32RF.cpp.
// Локальные переменные: int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static int16_t radarClampOverlayAbsX(int16_t value, int16_t width)
{
    const int16_t maxX = (int16_t)(kRadarOverlayScreenMaxX - width);
    return constrain(value, kRadarOverlayScreenMinX, maxX);
}

// Ограничивает Y-координату формуляра/подписи, чтобы текст не выходил за пределы экрана.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarClampOverlayAbsY` и обрабатывает радар clamp overlay abs y в контексте модуля ESP32RF.cpp.
// Локальные переменные: int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static int16_t radarClampOverlayAbsY(int16_t value, int16_t height)
{
    const int16_t maxY = (int16_t)(kRadarOverlayScreenMaxY - height);
    return constrain(value, kRadarOverlayScreenMinY, maxY);
}

// Нормализует угол радара к диапазону 0..359 градусов.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `normalizeRadarAngle360` и обрабатывает радар angle360 в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
static int16_t normalizeRadarAngle360(int16_t angleDeg)
{
    int16_t value = angleDeg % 360;  
    if (value < 0)
    {
        value += 360;
    }
    return value;
}

// Определяет предпочтительную сторону размещения формуляра по курсу цели.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarOverlaySideFromCourse` и обрабатывает радар overlay side курс в контексте модуля ESP32RF.cpp.
// Локальные переменные: int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

// Поворачивает сторону размещения формуляра по часовой стрелке.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarRotateSideCW` и обрабатывает радар rotate side cw в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
static RadarOverlaySide radarRotateSideCW(RadarOverlaySide side)
{
    return (RadarOverlaySide)((side + 1U) & 0x03U);
}

// Поворачивает сторону размещения формуляра против часовой стрелки.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarRotateSideCCW` и обрабатывает радар rotate side ccw в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
static RadarOverlaySide radarRotateSideCCW(RadarOverlaySide side)
{
    return (RadarOverlaySide)((side + 3U) & 0x03U);
}

// Возвращает сторону, противоположную текущей.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarOppositeSide` и обрабатывает радар opposite side в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
static RadarOverlaySide radarOppositeSide(RadarOverlaySide side)
{
    return (RadarOverlaySide)((side + 2U) & 0x03U);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarRectsOverlap` и обрабатывает радар rects overlap в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
static bool radarRectsOverlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                              int16_t bx, int16_t by, int16_t bw, int16_t bh,
                              int16_t padding)
{
    return !((ax + aw + padding) <= bx || (bx + bw + padding) <= ax ||
             (ay + ah + padding) <= by || (by + bh + padding) <= ay);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarRectOverlapsPlacedLabels` и обрабатывает радар rect overlaps placed labels в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarRectOverlapsPlacedSymbols` и обрабатывает радар rect overlaps placed symbols в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarPlaceOverlayBlockForSide` и обрабатывает радар place overlay block side в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarBuildOverlayLayout` и обрабатывает радар overlay layout в контексте модуля ESP32RF.cpp.
// Локальные переменные: int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; preferredSide — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; bestPenalty — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
//------------------------------------------------------------------------------
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

// Рисует графическую стрелку набора или снижения рядом с формуляром цели.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarDrawTrendArrowGlyph` и обрабатывает радар trend arrow glyph в контексте модуля ESP32RF.cpp.
// Локальные переменные: int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static void radarDrawTrendArrowGlyph(TFT_eSprite& sprite, int16_t x, int16_t y, bool isClimbing, uint16_t color)
{
    const int16_t centerX = x + (kRadarTrendArrowGlyphWidth / 2);

    if (isClimbing)
    {
        sprite.fillTriangle(centerX, y,
                            x, y + 3,
                            x + kRadarTrendArrowGlyphWidth - 1, y + 3,
                            color);
        sprite.drawLine(centerX, y + 3, centerX, y + kRadarTrendArrowGlyphHeight - 1, color);
    }
    else
    {
        sprite.drawLine(centerX, y, centerX, y + kRadarTrendArrowGlyphHeight - 4, color);
        sprite.fillTriangle(centerX, y + kRadarTrendArrowGlyphHeight - 1,
                            x, y + kRadarTrendArrowGlyphHeight - 4,
                            x + kRadarTrendArrowGlyphWidth - 1, y + kRadarTrendArrowGlyphHeight - 4,
                            color);
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarDrawTargetDirect` и обрабатывает радар цель direct в контексте модуля ESP32RF.cpp.
// Локальные переменные: int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; dirRad — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; dirX — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; dirY — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; normX — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; normY — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
//------------------------------------------------------------------------------
static void radarDrawTargetDirect(TFT_eSprite& sprite, int16_t centerX, int16_t centerY, int16_t courseDeg, int16_t speedView, uint16_t color)
{
    sprite.fillCircle(centerX, centerY, 6, color);

    const int16_t clampedSpeedView = constrain((int32_t)speedView, 20, 40);
    const float dirRad = ((float)((courseDeg % 360 + 360) % 360)) * 0.01745329252f;
    const float dirX = sinf(dirRad);
    const float dirY = -cosf(dirRad);
    const float normX = cosf(dirRad);
    const float normY = sinf(dirRad);

    const float startDist = 5.0f;  
    const float endDist = startDist + (float)(49 - clampedSpeedView);

    for (int8_t offset = -1; offset <= 1; ++offset)
    {
        const int16_t x0 = (int16_t)lroundf((float)centerX + dirX * startDist + normX * offset);
        const int16_t y0 = (int16_t)lroundf((float)centerY + dirY * startDist + normY * offset);
        const int16_t x1 = (int16_t)lroundf((float)centerX + dirX * endDist + normX * offset);
        const int16_t y1 = (int16_t)lroundf((float)centerY + dirY * endDist + normY * offset);
        sprite.drawLine(x0, y0, x1, y1, color);
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarDrawLabelDirect` и обрабатывает радар label direct в контексте модуля ESP32RF.cpp.
// Локальные переменные: safeText — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static void radarDrawLabelDirect(TFT_eSprite& sprite, int16_t x, int16_t y, const char* text, uint8_t trendArrow, uint16_t color, bool labelNearTargetOnRight)
{
    const char* safeText = (text != nullptr) ? text : "";

    /* Текст формуляра выводим через явный встроенный шрифт №2.
       Для формуляра, расположенного слева от точки самолета, текст поджимаем
       к правому краю блока, чтобы он был визуально ближе к самой точке. */
    sprite.setTextDatum(TL_DATUM);
    sprite.setTextFont(2);
    sprite.setTextSize(1);
    sprite.setTextColor(color, backColor);

    const int16_t textWidthPx = (int16_t)sprite.textWidth(safeText, 2);
    const int16_t reserveArrowPx = (trendArrow == 1 || trendArrow == 2)
        ? (kRadarTrendArrowGlyphWidth + kRadarTrendArrowGlyphGapPx)
        : 0;

    int16_t textX = x + 2;  
    if (!labelNearTargetOnRight)
    {
        textX = x + kRadarLabelSpriteWidth - textWidthPx - reserveArrowPx - 1;
        if (textX < x + 2)
        {
            textX = x + 2;
        }
    }

    sprite.drawString(safeText, textX, y + 1, 2);

    if (trendArrow == 1 || trendArrow == 2)
    {
        int16_t arrowX = textX + textWidthPx + kRadarTrendArrowGlyphGapPx;
        const int16_t maxArrowX = x + kRadarLabelSpriteWidth - kRadarTrendArrowGlyphWidth;
        if (arrowX > maxArrowX)
        {
            arrowX = maxArrowX;
        }
        radarDrawTrendArrowGlyph(sprite, arrowX, y + 3, trendArrow == 1, color);
    }
}

// Рисует символ нашего самолета в центре радара.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `radarDrawOwnshipSymbol` и обрабатывает радар ownship symbol в контексте модуля ESP32RF.cpp.
// Локальные переменные: x — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; y — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
//------------------------------------------------------------------------------
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

// Возвращает время работы устройства в секундах с момента запуска.
//------------------------------------------------------------------------------
// Назначение функции: Выводит uptime seconds пользователю на экран или в текстовый блок интерфейса.

//------------------------------------------------------------------------------
static time_t displayUptimeSeconds()
{
    return (time_t)(millis() / 1000UL);
}

// Подсчитывает количество актуальных сторонних самолетов в контейнере.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `alien_count` и обрабатывает alien count в контексте модуля ESP32RF.cpp.
// Локальные переменные: count — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора.
//------------------------------------------------------------------------------
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

// Проверяет, находится ли система в ожидании валидных координат нашего самолета.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `coordinates_waiting` и обрабатывает coordinates waiting в контексте модуля ESP32RF.cpp.

//------------------------------------------------------------------------------
bool coordinates_waiting()
{
    aux_t aux = {};
    RS485Display_getIncomingAux(&aux);
    return aux.gps_waiting_M;
}

// Рисует служебное сообщение о состоянии GNSS: ожидание, поиск или отсутствие данных.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает данные GNSS status overlay на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: waitingGps — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; noGpsData — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; boxX — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; boxY — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; boxW — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; boxH — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
//------------------------------------------------------------------------------
static void waiting_txt() // Вывод текста ожидания определения координат
{
    location_Message.createSprite(300, 100);
    location_Message.fillSprite(TFT_BLACK);
    location_Message.setTextColor(TFT_YELLOW, TFT_BLACK);
    location_Message.setTextDatum(CC_DATUM);
    location_Message.loadFont(AA_FONT_TIME28I);
    location_Message.drawString("ОПРЕДЕЛЕНИЕ", 150, 15);
    location_Message.drawString("МЕСТОПОЛОЖЕНИЯ", 150, 48);
    location_Message.drawString("ОЖИДАЙТЕ", 150, 81);
    location_Message.pushToSprite(&back, 90, 70, TFT_BLACK);
    location_Message.unloadFont();
    location_Message.deleteSprite();
}

static void drawGnssStatusOverlay()
{
    if (Remote_baseTestMode())
    {
        return;
    }

    aux_t aux = {};
    RS485Display_getIncomingAux(&aux);
    const bool showGnssWait = aux.gps_waiting_M || aux.gps_no_data_M;
    if (!showGnssWait)
    {
        return;
    }

    // Внешний дисплей синхронизируется по RS485 с базовым модулем:
    // надпись появляется и пропадает в тот же момент, что и на базе.
    waiting_txt();
}

//==============================================================================

// Отрисовывает верхний левый блок времени и связанных кратких статусов.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает top left время block на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: timeBuf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - timeBuf: Временная отметка, интервал или значение тайм-аута.
//------------------------------------------------------------------------------
static uint8_t powerBatteryPercentFromVoltage(float voltage)
{
    if (voltage <= INA219_BATTERY_MIN_V) return 0;
    if (voltage >= INA219_BATTERY_MAX_V) return 100;
    const float span = INA219_BATTERY_MAX_V - INA219_BATTERY_MIN_V;
    if (span <= 0.01f) return 0;
    return (uint8_t)constrain((int)lroundf(((voltage - INA219_BATTERY_MIN_V) * 100.0f) / span), 0, 100);
}

static bool powerProbeIna219()
{
    Wire.begin();
    Wire.beginTransmission(INA219_ADDRESS);
    if (Wire.endTransmission() != 0)
    {
        return false;
    }
    g_ina219.begin(&Wire);
    g_ina219.setCalibration_32V_2A();
    return true;
}

static void updatePowerTelemetry()
{
    const uint32_t nowMs = millis();
    if (!g_ina219Ready)
    {
        if (g_ina219LastProbeMs != 0 && (uint32_t)(nowMs - g_ina219LastProbeMs) < 5000UL)
        {
            return;
        }
        g_ina219LastProbeMs = nowMs;
        g_ina219Ready = powerProbeIna219();
        if (!g_ina219Ready)
        {
            g_powerTelemetryValid = false;
            return;
        }
        g_ina219LastReadMs = 0;
    }

    if (g_ina219LastReadMs != 0 && (uint32_t)(nowMs - g_ina219LastReadMs) < 1000UL)
    {
        return;
    }
    g_ina219LastReadMs = nowMs;

    digitalWrite(POWER_ON_PIN, LOW);
    delay(20);
    const float busVoltage = g_ina219.getBusVoltage_V();
    const float shuntVoltage = g_ina219.getShuntVoltage_mV();
    float currentMa = g_ina219.getCurrent_mA();
    digitalWrite(POWER_ON_PIN, HIGH);

    if (currentMa < 0.0f) currentMa = -currentMa;
    const float loadVoltage = busVoltage + (shuntVoltage / 1000.0f);

    if (!(busVoltage >= 0.0f && busVoltage < 40.0f &&
          shuntVoltage > -1000.0f && shuntVoltage < 1000.0f &&
          currentMa >= 0.0f && currentMa < 10000.0f &&
          loadVoltage > 0.0f && loadVoltage < 40.0f))
    {
        g_powerTelemetryValid = false;
        return;
    }

    g_powerVoltageV = loadVoltage;
    g_powerCurrentMa = currentMa;
    g_powerBatteryPercent = powerBatteryPercentFromVoltage(loadVoltage);
    g_powerTelemetryValid = true;
}

static void drawBatteryIcon(int x, int y, uint8_t percent, bool valid)
{
    const int bodyW = 34;
    const int bodyH = 16;
    const int capW = 4;
    const uint16_t borderColor = valid ? TFT_LIGHTGREY : TFT_DARKGREY;
    uint16_t fillColor = TFT_DARKGREY;
    if (valid)
    {
        fillColor = (percent > 50) ? TFT_GREEN : ((percent > 20) ? TFT_YELLOW : TFT_RED);
    }

    back.drawRect(x, y, bodyW, bodyH, borderColor);
    back.fillRect(x + bodyW, y + 4, capW, bodyH - 8, borderColor);

    const int gap = 2;
    const int segW = (bodyW - 10) / 4;
    const int segH = bodyH - 6;
    const int level = valid ? constrain((int)((percent + 24U) / 25U), 0, 4) : 0;
    for (int i = 0; i < 4; ++i)
    {
        const int sx = x + 3 + i * (segW + gap);
        if (i < level)
        {
            back.fillRect(sx, y + 3, segW, segH, fillColor);
        }
        else
        {
            back.drawRect(sx, y + 3, segW, segH, TFT_DARKGREY);
        }
    }
}

static void drawTopRightPowerBlock()
{
    if (settings == nullptr)
    {
        return;
    }
    const bool showBattery = settings->power_battery_view != 0;
    const bool showVoltage = settings->power_voltage_view != 0;
    const bool showCurrent = settings->power_current_view != 0;
    if (!showBattery && !showVoltage && !showCurrent)
    {
        return;
    }

    updatePowerTelemetry();

    back.setTextFont(2);
    back.setTextSize(1);
    back.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

    char lineBuf[40];
    const int xBattery = 398;
    const int y = 4;

    // Ток и напряжение выводим в верхней строке перед изображением батарейки.
    if (showVoltage || showCurrent)
    {
        if (showVoltage && showCurrent)
        {
            if (g_powerTelemetryValid) snprintf(lineBuf, sizeof(lineBuf), "%.2f V  %.0f mA", (double)g_powerVoltageV, (double)g_powerCurrentMa);
            else snprintf(lineBuf, sizeof(lineBuf), "--.-- V  --- mA");
        }
        else if (showVoltage)
        {
            if (g_powerTelemetryValid) snprintf(lineBuf, sizeof(lineBuf), "%.2f V", (double)g_powerVoltageV);
            else snprintf(lineBuf, sizeof(lineBuf), "--.-- V");
        }
        else
        {
            if (g_powerTelemetryValid) snprintf(lineBuf, sizeof(lineBuf), "%.0f mA", (double)g_powerCurrentMa);
            else snprintf(lineBuf, sizeof(lineBuf), "--- mA");
        }
        back.setTextDatum(TR_DATUM);
        back.drawString(lineBuf, showBattery ? (xBattery - 6) : 478, y, 2);
    }

    if (showBattery)
    {
        drawBatteryIcon(xBattery, y, g_powerBatteryPercent, g_powerTelemetryValid);
        back.setTextDatum(TL_DATUM);
        if (g_powerTelemetryValid)
        {
            snprintf(lineBuf, sizeof(lineBuf), "%u%%", (unsigned)g_powerBatteryPercent);
        }
        else
        {
            snprintf(lineBuf, sizeof(lineBuf), "--%%");
        }
        back.drawString(lineBuf, 439, y + 1, 2);
    }
    back.setTextDatum(TL_DATUM);
}

static void drawTopLeftTimeBlock()
{
    back.setTextDatum(TL_DATUM);
    back.setTextColor(TFT_GREEN, TFT_BLACK);
    back.loadFont(AA_FONT_TIME24);

    char timeBuf[24];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    if (Remote_baseTestMode())
    {
        snprintf(timeBuf, sizeof(timeBuf), "-:-");
    }
    else if (GNSS_timeValid())
    {
        snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u", (unsigned)GNSS_hour(), (unsigned)GNSS_minute());
  /*      const time_t t = now();
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hour(t), minute(t));*/
    }
    else
    {
        snprintf(timeBuf, sizeof(timeBuf), "--:--");
    }

    back.drawString(timeBuf, 6, 4);
    back.unloadFont();
    back.setTextDatum(TL_DATUM);
}

// Копирует из UTF-8 строки не более заданного количества символов, не ломая многобайтные буквы.
//------------------------------------------------------------------------------
// Назначение функции: Копирует utf8 prefix chars в нужную структуру или буфер, сохраняя текущие данные для дальнейшего использования.
// Локальные переменные: p — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; bytesOut — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; charsOut — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static size_t copyUtf8PrefixByChars(const char* src, size_t maxChars, char* dst, size_t dstSize)
{
    if (dst == nullptr || dstSize == 0)
    {
        return 0;
    }

    dst[0] = '\0';
    if (src == nullptr || *src == '\0' || maxChars == 0)
    {
        return 0;
    }

    const unsigned char* p = reinterpret_cast<const unsigned char*>(src);
    size_t bytesOut = 0;  
    size_t charsOut = 0;  

    while (*p != 0 && charsOut < maxChars)
    {
        size_t cpLen = 1;  
        if ((*p & 0x80U) == 0x00U) cpLen = 1;
        else if ((*p & 0xE0U) == 0xC0U) cpLen = 2;
        else if ((*p & 0xF0U) == 0xE0U) cpLen = 3;
        else if ((*p & 0xF8U) == 0xF0U) cpLen = 4;

        if (bytesOut + cpLen >= dstSize)
        {
            break;
        }

        for (size_t i = 0; i < cpLen && p[i] != 0; ++i)
        {
            dst[bytesOut++] = (char)p[i];
        }
        p += cpLen;
        ++charsOut;
    }

    dst[bytesOut] = '\0';
    return bytesOut;
}

// Возвращает длину одного UTF-8 символа в байтах.
// Нужен для корректной кириллицы при переносе текста.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerUtf8CodepointLen` и обрабатывает трекер utf8 codepoint len в контексте модуля ESP32RF.cpp.
// Локальные переменные: uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static size_t trackerUtf8CodepointLen(const char* p)
{
    if (p == nullptr || *p == '\0')
    {
        return 0U;
    }

    const uint8_t c = static_cast<uint8_t>(*p);
    if ((c & 0x80U) == 0x00U) return 1U;
    if ((c & 0xE0U) == 0xC0U) return 2U;
    if ((c & 0xF0U) == 0xE0U) return 3U;
    if ((c & 0xF8U) == 0xF0U) return 4U;
    return 1U;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `wrapTrackerMessageLines` и обрабатывает wrap трекер текстовое сообщение lines в контексте модуля ESP32RF.cpp.
// Локальные переменные: lineIndex — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; p — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - current: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - candidate: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
static size_t wrapTrackerMessageLines(TFT_eSprite& spr, const char* text, char lines[][160], size_t maxLines, int maxWidth)
{
    if (text == nullptr || text[0] == '\0' || maxLines == 0U)
    {
        return 0U;
    }

    for (size_t i = 0; i < maxLines; ++i)
    {
        lines[i][0] = '\0';
    }

    size_t lineIndex = 0U;  
    const char* p = text;  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.

    while (*p != '\0' && lineIndex < maxLines)
    {
        char current[160] = {};  
        size_t currentLen = 0U;  
        const char* lastSpaceSrc = nullptr;  
        size_t lastSpaceLen = 0U;  

        while (*p != '\0')
        {
            if (*p == '\n')
            {
                ++p;
                break;
            }

            const size_t cpLen = trackerUtf8CodepointLen(p);
            if (cpLen == 0U || currentLen + cpLen >= sizeof(current))
            {
                break;
            }

            char candidate[160] = {};
            if (currentLen > 0U)
            {
                memcpy(candidate, current, currentLen);
            }
            memcpy(candidate + currentLen, p, cpLen);
            candidate[currentLen + cpLen] = '\0';

            if (currentLen > 0U && spr.textWidth(candidate) > maxWidth)
            {
                if (lastSpaceSrc != nullptr && lastSpaceLen > 0U)
                {
                    current[lastSpaceLen] = '\0';
                    p = lastSpaceSrc;
                }
                break;
            }

            memcpy(current + currentLen, p, cpLen);
            currentLen += cpLen;
            current[currentLen] = '\0';

            if (*p == ' ')
            {
                lastSpaceSrc = p + cpLen;
                lastSpaceLen = currentLen;
            }

            p += cpLen;

            if (spr.textWidth(current) > maxWidth)
            {
                break;
            }
        }

        while (currentLen > 0U && current[currentLen - 1U] == ' ')
        {
            current[--currentLen] = '\0';
        }

        strncpy(lines[lineIndex], current, 159U);
        lines[lineIndex][159U] = '\0';
        ++lineIndex;

        while (*p == ' ')
        {
            ++p;
        }
    }

    if (*p != '\0' && lineIndex > 0U)
    {
        char* last = lines[lineIndex - 1U];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        const size_t len = strlen(last);
        if (len + 3U < 160U)
        {
            strcat(last, "...");
        }
    }

    return lineIndex;
}

// Определяет, должен ли блок текстового сообщения сейчас быть видимым.
// Реализует режим мигания: показ и скрытие по таймеру.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerMessageBlinkVisible` и обрабатывает трекер текстовое сообщение мигание visible в контексте модуля ESP32RF.cpp.
// Локальные переменные: lastActive — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных; active — логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные; uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - lastMessage: Временная отметка, интервал или значение тайм-аута.
//------------------------------------------------------------------------------
static bool trackerMessageBlinkVisible(const char* message)
{
    static char lastMessage[BUFFER_SIZE] = {};  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    static bool lastActive = false;  

    const bool active = (message != nullptr && message[0] != '\0');
    const uint32_t nowMs = millis();

    if (!active)
    {
        lastActive = false;
        lastMessage[0] = '\0';
        isMessageVisible = false;
        isMessageDeleted = false;
        lastToggle = nowMs;
        return false;
    }

    if (!lastActive || strcmp(lastMessage, message) != 0)
    {
        strncpy(lastMessage, message, sizeof(lastMessage) - 1U);
        lastMessage[sizeof(lastMessage) - 1U] = '\0';
        lastActive = true;
        isMessageDeleted = false;
        isMessageVisible = true;
        lastToggle = nowMs;
    }

    const uint32_t period = isMessageVisible ? showPeriod : hidePeriod;   
    if ((uint32_t)(nowMs - lastToggle) >= period)
    {
        isMessageVisible = !isMessageVisible;
        lastToggle = nowMs;
    }

    return isMessageVisible;
}

// Рисует текстовое сообщение от трекера под часами на TFT.
// Сообщение может быть многострочным, мигающим и кириллическим.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает трекер текст текстовое сообщение block на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: message — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных; 
// boxX — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// boxY — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// boxW — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
// boxH — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; 
//   maxWidth — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
// - lines: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
static void drawTrackerTextMessageBlock()
{
    const char* message = Tracker_getActiveTextMessage();
    if (!Tracker_hasActiveTextMessage() || message == nullptr || message[0] == '\0')
    {
        (void)trackerMessageBlinkVisible(nullptr);
        if (rows_Message.created())
        {
            rows_Message.deleteSprite();
        }
        return;
    }

    if (!trackerMessageBlinkVisible(message))
    {
        if (rows_Message.created())
        {
            rows_Message.deleteSprite();
        }
        return;
    }

    const int boxX = 0;  
    const int boxY = 25;  
    const int boxW = 480;  
    const int boxH = 160;  

    if (rows_Message.created())
    {
        rows_Message.deleteSprite();
    }

    rows_Message.createSprite(boxW, boxH);
    rows_Message.fillSprite(TFT_BLACK);
    rows_Message.setTextColor(TFT_YELLOW, TFT_BLACK);
    rows_Message.setTextDatum(TL_DATUM);
    rows_Message.setAttribute(UTF8_SWITCH, true);
    rows_Message.loadFont(AA_FONT_TIME28);

    char lines[5][160] = {};
    const int maxWidth = boxW - 6;  
    const size_t lineCount = wrapTrackerMessageLines(rows_Message, message, lines, 5U, maxWidth);
    const int lineStep = rows_Message.fontHeight() + 2;

    for (size_t i = 0; i < lineCount; ++i)
    {
        rows_Message.drawString(lines[i], 2, 2 + ((int)i * lineStep));
    }

    rows_Message.unloadFont();
    rows_Message.pushToSprite(&back, boxX, boxY, TFT_BLACK);
    rows_Message.deleteSprite();
}

// Возвращает последнее доступное значение RSSI LoRa для нижнего информационного блока.
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `latestLoRaRssiValue` и обрабатывает latest lo ra rssi value в контексте модуля ESP32RF.cpp.
// Локальные переменные: bestRssi — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; found — структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении; freshestTs — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; nowSec — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static int latestLoRaRssiValue()
{
    int bestRssi = 0;  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    bool found = false;  
    time_t freshestTs = 0;  
    const time_t nowSec = displayUptimeSeconds();

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        if (!Container[i].addr)
        {
            continue;
        }
        if ((nowSec - Container[i].timestamp) > TFT_EXPIRATION_TIME)
        {
            continue;
        }
        if (Container[i].signal_source != TRAFFIC_SOURCE_FLARM_LORA)
        {
            continue;
        }
        if (Container[i].rssi_LoRa >= 0)
        {
            continue;
        }
        if (!found || Container[i].timestamp > freshestTs)
        {
            freshestTs = Container[i].timestamp;
            bestRssi = (int)Container[i].rssi_LoRa;
            found = true;
        }
    }

    return found ? bestRssi : 0;
}

// Отрисовывает нижний левый блок статуса LoRa: счетчики, RSSI и связанные параметры.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает bottom left lo ra block на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: x — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; y0 — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; dy — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lineBuf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; rssiValue — параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние rf.
// - lineBuf: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
static void drawBottomLeftLoRaBlock()
{
    if (!(settings && settings->rssi_view == VIEW_RSSI_ON))
    {
        return;
    }

    back.setTextFont(2);
    back.setTextSize(1);
    back.setTextDatum(TL_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);

    const int x = 2;  
    const int y0 = 257;  
    const int dy = 15;  
    char lineBuf[64];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.

    const uint32_t txPackets = Remote_loraTxPackets();
    const uint32_t rxPackets = Remote_loraRxPackets();
    snprintf(lineBuf, sizeof(lineBuf), "LoRa Tx %lu", (unsigned long)txPackets);
    back.drawString(lineBuf, x, y0, 2);
    snprintf(lineBuf, sizeof(lineBuf), "LoRa Rx %lu", (unsigned long)rxPackets);
    back.drawString(lineBuf, x, y0 + dy, 2);

    const uint32_t rfFreqHz = Remote_loraRfHz();
    if (rfFreqHz > 0UL)
    {
        const uint32_t mhz = rfFreqHz / 1000000UL;  
        const uint32_t khz = (rfFreqHz % 1000000UL) / 1000UL;
        snprintf(lineBuf, sizeof(lineBuf), "LoRa RF %lu.%03lu", (unsigned long)mhz, (unsigned long)khz);
    }
    else
    {
        snprintf(lineBuf, sizeof(lineBuf), "LoRa RF ---");
    }
    back.drawString(lineBuf, x, y0 + dy * 2, 2);

    const int rssiValue = latestLoRaRssiValue();
    if (rssiValue < 0)
    {
        snprintf(lineBuf, sizeof(lineBuf), "LoRa RSSI %d dB", rssiValue);
    }
    else
    {
        snprintf(lineBuf, sizeof(lineBuf), "LoRa RSSI ---");
    }
    back.drawString(lineBuf, x, y0 + dy * 3, 2);
}

// Проверяет, работает ли устройство в одном из тестовых режимов.
//------------------------------------------------------------------------------
// Назначение функции: Возвращает device test режим, рассчитанное или считанное по текущему состоянию модуля.

//------------------------------------------------------------------------------
static bool isDeviceTestMode()
{
    return Remote_baseTestMode();
}

// Проверяет, заданы ли локальные координаты для тестовых режимов.
//------------------------------------------------------------------------------
// Назначение функции: Возвращает configured local coordinates, рассчитанное или считанное по текущему состоянию модуля.

//------------------------------------------------------------------------------
static bool hasConfiguredLocalCoordinates()
{
    return Remote_baseCoordinateValid() && Remote_baseCoordinateIsLocal();
}

// Определяет, нужно ли выводить блок локальных координат в правом нижнем углу.
//------------------------------------------------------------------------------
// Назначение функции: Возвращает test coordinates block, рассчитанное или считанное по текущему состоянию модуля.

//------------------------------------------------------------------------------
static bool shouldDrawTestCoordinatesBlock()
{
    // В тестовых режимах в правом нижнем углу выводим только локальные координаты.
    return (settings == nullptr || settings->gps_state_view != 0) && isDeviceTestMode();
}

// Определяет, нужно ли выводить блок GNSS-координат в правом нижнем углу.
//------------------------------------------------------------------------------
// Назначение функции: Возвращает данные GNSS block, рассчитанное или считанное по текущему состоянию модуля.

//------------------------------------------------------------------------------
static bool shouldDrawLanBlock()
{
    return settings != nullptr && settings->lan_state_view != 0 && Remote_lanStatusReceived();
}

static bool shouldDrawGnssBlock()
{
    if (settings != nullptr && settings->gps_state_view == 0)
    {
        return false;
    }

    // В нормальном режиме в правом нижнем углу выводим только данные GPS.
    if (isDeviceTestMode())
    {
        return false;
    }

    aux_t aux = {};
    RS485Display_getIncomingAux(&aux);
    return Remote_baseGpsRx() || Remote_baseCoordinateValid() || GNSS_coordinatesValid() || GNSS_satellitesValid() || GNSS_timeValid() ||
           aux.gps_waiting_M || aux.gps_no_data_M;
}

// Рисует правый нижний блок с GNSS-координатами, временем и спутниками.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает bottom right данные GNSS block на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: xRight — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; yTop — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; dy — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lineBuf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - lineBuf: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
static void drawBottomRightLanBlock()
{
    if (!shouldDrawLanBlock())
    {
        return;
    }

    back.setTextFont(2);
    back.setTextSize(1);
    back.setTextDatum(TR_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);

    const int xRight = 478;
    const int yTop = 214;
    const int dy = 15;
    char lineBuf[64];

    const char* addrModeText = "Static";
    if (!Remote_lanReady()) addrModeText = "Off";
    else if (Remote_lanDhcp()) addrModeText = "DHCP";
    else if (Remote_lanLinkUp()) addrModeText = "DHCP...";
    back.drawString(addrModeText, xRight, yTop, 2);

    uint8_t lanIp[4] = {};
    Remote_lanIp(lanIp);
    snprintf(lineBuf, sizeof(lineBuf), "IP %u.%u.%u.%u",
             (unsigned)lanIp[0], (unsigned)lanIp[1], (unsigned)lanIp[2], (unsigned)lanIp[3]);
    back.drawString(lineBuf, xRight, yTop + dy, 2);

    const int yState = yTop + dy * 2;
    const char* lanLabel = "LAN ";
    const char* udpLabel = "  UDP ";
    const char* lanStateWord = Remote_lanLinkUp() ? "On" : "Off";
    const char* udpStateWord = Remote_lanUdpWorking() ? "On" : "Off";
    int totalW = back.textWidth(lanLabel, 2) + back.textWidth(lanStateWord, 2) +
                 back.textWidth(udpLabel, 2) + back.textWidth(udpStateWord, 2);
    int xText = xRight - totalW;

    back.setTextDatum(TL_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
    back.drawString(lanLabel, xText, yState, 2);
    xText += back.textWidth(lanLabel, 2);
    back.setTextColor(Remote_lanLinkUp() ? TFT_GREEN : TFT_RED, TFT_BLACK);
    back.drawString(lanStateWord, xText, yState, 2);
    xText += back.textWidth(lanStateWord, 2);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
    back.drawString(udpLabel, xText, yState, 2);
    xText += back.textWidth(udpLabel, 2);
    back.setTextColor(Remote_lanUdpWorking() ? TFT_GREEN : TFT_RED, TFT_BLACK);
    back.drawString(udpStateWord, xText, yState, 2);

    back.setTextDatum(TR_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);
    snprintf(lineBuf, sizeof(lineBuf), "Tx %lu  Rx %lu",
             (unsigned long)Remote_lanTxPackets(), (unsigned long)Remote_lanRxPackets());
    back.drawString(lineBuf, xRight, yTop + dy * 3, 2);
    back.setTextDatum(TL_DATUM);
}

static void drawBottomRightGnssBlock()
{
    if (!shouldDrawGnssBlock())
    {
        return;
    }

    back.setTextFont(2);
    back.setTextSize(1);
    back.setTextDatum(TR_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);

    const int xRight = 478;  
    // Блок GPS смещен ниже экрана, так как в нормальном режиме он выводится один.
    const int yTop = 274;  
    const int dy = 14;  
    char lineBuf[64];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.

    if (GNSS_satellitesValid())
    {
        snprintf(lineBuf, sizeof(lineBuf), "GPS Sat %u", (unsigned)GNSS_satellites());
    }
    else
    {
        snprintf(lineBuf, sizeof(lineBuf), "GPS Sat --");
    }
    back.drawString(lineBuf, xRight, yTop, 2);

    const bool coordValid = GNSS_coordinatesValid();
    if (coordValid)
    {
        snprintf(lineBuf, sizeof(lineBuf), "GPS Lat %.5f", (double)Remote_baseLatitude());
    }
    else
    {
        snprintf(lineBuf, sizeof(lineBuf), "GPS Lat ---");
    }
    back.drawString(lineBuf, xRight, yTop + dy, 2);

    if (coordValid)
    {
        snprintf(lineBuf, sizeof(lineBuf), "GPS Lon %.5f", (double)Remote_baseLongitude());
    }
    else
    {
        snprintf(lineBuf, sizeof(lineBuf), "GPS Lon ---");
    }
    back.drawString(lineBuf, xRight, yTop + dy * 2, 2);

    back.setTextDatum(TL_DATUM);
}

// Рисует правый нижний блок локальных тестовых координат.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает bottom right test coordinates block на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.
// Локальные переменные: testLat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; testLon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; xRight — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; yTop — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; dy — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lineBuf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - lineBuf: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
static void drawBottomRightTestCoordinatesBlock()
{
    if (!shouldDrawTestCoordinatesBlock())
    {
        return;
    }

    back.setTextFont(2);
    back.setTextSize(1);
    back.setTextDatum(TR_DATUM);
    back.setTextColor(TFT_DARKGREY, TFT_BLACK);

    const int xRight = 478;  
    // В тестовых режимах локальные координаты располагаем максимально низко.
    const int yTop = 289;  
    const int dy = 14;  
    char lineBuf[64];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.

    if (hasConfiguredLocalCoordinates())
    {
        snprintf(lineBuf, sizeof(lineBuf), "Local Lat %.5f", (double)Remote_baseLatitude());
    }
    else
    {
        snprintf(lineBuf, sizeof(lineBuf), "Local Lat ---");
    }
    back.drawString(lineBuf, xRight, yTop, 2);

    if (hasConfiguredLocalCoordinates())
    {
        snprintf(lineBuf, sizeof(lineBuf), "Local Lon %.5f", (double)Remote_baseLongitude());
    }
    else
    {
        snprintf(lineBuf, sizeof(lineBuf), "Local Lon ---");
    }
    back.drawString(lineBuf, xRight, yTop + dy, 2);

    back.setTextDatum(TL_DATUM);
}

// Собирает и выводит общий блок статусной информации поверх радара.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает status info block на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.

//------------------------------------------------------------------------------
static void drawStatusInfoBlock()
{
    drawTopLeftTimeBlock();
    drawTopRightPowerBlock();
    drawBottomLeftLoRaBlock();
    drawBottomRightLanBlock();
    drawBottomRightGnssBlock();
    drawBottomRightTestCoordinatesBlock();
}

// Выполняет базовую отрисовку радарного поля и постоянных статусных блоков.
//------------------------------------------------------------------------------
// Назначение функции: Отрисовывает радар базу status на дисплее или в промежуточном спрайте с учетом текущих параметров отображения.

//------------------------------------------------------------------------------
static void drawRadarBaseAndStatus()
{
    Draw_circular_scale();
    esp_task_wdt_reset();

    /* Выполняем поворот базовой шкалы по азимуту */
    backsprite.pushRotated(&back, angle, TFT_BLACK);
    esp_task_wdt_reset();

    if (divider <= 32767)
    {
        back.setTextFont(2);
        back.setTextSize(1);
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

    drawStatusInfoBlock();

    /* Наш самолёт рисуем после служебных текстов, чтобы он тоже был сверху. */
    radarDrawOwnshipSymbol(back);
    esp_task_wdt_reset();
}

// Основной цикл модуля TFT.
// Обновляет экран, обрабатывает кнопки, текстовые сообщения и перерисовку целей.
//------------------------------------------------------------------------------
// Назначение функции: Обслуживает служебную операцию модуля в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: buf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - buf: Буфер, текстовая строка или рабочее сообщение.
// - disp_value: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - tbw: Параметр геометрии, координаты, размера или угла.
// - tbh: Параметр геометрии, координаты, размера или угла.
// - dimension_array_speed: Числовой параметр навигации, радиообмена, геометрии или измерения.
// - dimension_array_altitude: Числовой параметр навигации, радиообмена, геометрии или измерения.
// - placedSymbols: Параметр геометрии, координаты, размера или угла.
// - placedLabels: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - heightText: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
static bool isBaseLinkActive()
{
    const uint32_t lastRxMs = RS485Display_lastRxMs();
    return (lastRxMs != 0U) && ((uint32_t)(millis() - lastRxMs) < 3000UL);
}

static void drawNoBaseConnectionOverlayIfNeeded()
{
    if (isBaseLinkActive())
    {
        return;
    }

    // Аварийная надпись выводится поверх всех остальных текстов кадра.
    back.fillRoundRect(18, 126, 444, 68, 8, TFT_BLACK);
    back.drawRoundRect(18, 126, 444, 68, 8, TFT_RED);
    back.setAttribute(UTF8_SWITCH, true);
    back.loadFont(AA_FONT_TIME28);
    back.setTextDatum(MC_DATUM);
    back.setTextColor(TFT_RED, TFT_BLACK);
    back.setTextSize(1);
    back.drawString("НЕТ СВЯЗИ С БАЗОЙ", 240, 160);
    back.unloadFont();
    back.setTextDatum(TL_DATUM);
}

static bool readSosInputActive()
{
    if (!isBaseLinkActive())
    {
        return false;
    }
    aux_t aux = {};
    RS485Display_getIncomingAux(&aux);
    return aux.new_SOS_flag_M;
}

//------------------------------------------------------------------------------
// Назначение функции: Обновляет флаги SOS и источника координат для пакета внешнего дисплея.
//------------------------------------------------------------------------------
static void updateExternalDisplayStatusFlags()
{
    const bool sosActiveNow = readSosInputActive();
    g_sosInputActive = sosActiveNow;

    // При LOW на GPIO42 полностью сбрасываем индикацию SOS и не оставляем
    // старые флаги в пакете внешнего дисплея.
    if (!sosActiveNow)
    {
        g_sosOverlayVisible = false;
        g_sosBlinkCycleStartMs = 0U;
    }

    // Внешний дисплей не формирует SOS сам: флаг приходит от базы по RS485.
    // В ответный пакет его не записываем, чтобы не исказить состояние базы.
    (void)sosActiveNow;
}

//------------------------------------------------------------------------------
// Назначение функции: Рисует крупную надпись SOS в центре TFT при активном входе GPIO42.
//------------------------------------------------------------------------------
static bool sosOverlayEnabledAndActive()
{
    // GPIO42 проверяется непосредственно перед выводом, чтобы надпись SOS
    // появлялась сразу при HIGH даже если состояние изменилось между кадрами.
    const bool sosActiveNow = readSosInputActive();
    g_sosInputActive = sosActiveNow;

    if (!sosActiveNow)
    {
        g_sosOverlayVisible = false;
        g_sosBlinkCycleStartMs = 0U;
        return false;
    }

    if (settings == nullptr || settings->display_sos == 0)
    {
        g_sosOverlayVisible = false;
        return false;
    }

    const uint32_t nowMs = millis();
    if (g_sosBlinkCycleStartMs == 0U)
    {
        g_sosBlinkCycleStartMs = nowMs;
    }

    const uint32_t cycleMs = SOS_BLINK_ON_MS + SOS_BLINK_OFF_MS;
    uint32_t phaseMs = nowMs - g_sosBlinkCycleStartMs;
    if (phaseMs >= cycleMs)
    {
        g_sosBlinkCycleStartMs = nowMs;
        phaseMs = 0U;
    }

    // 4 секунды отображаем, 1 секунду не отображаем.
    g_sosOverlayVisible = (phaseMs < SOS_BLINK_ON_MS);
    return g_sosOverlayVisible;
}

static void drawSosOverlayIfNeeded()
{
    if (!sosOverlayEnabledAndActive()) return;

    // SOS выводится в общий back-спрайт ДО back.pushSprite(),
    // как текстовое сообщение трекера. Поэтому готовый кадр TFT
    // отправляется уже вместе с надписью, и основной экран не затирает SOS.
    back.setAttribute(UTF8_SWITCH, true);
    back.loadFont(AA_FONT_TIME48);
    back.setTextDatum(MC_DATUM);
    back.setTextColor(TFT_YELLOW, TFT_BLACK);
    back.setTextSize(1);
    back.drawString("SOS", 240, 160);
    back.unloadFont();
    back.setTextDatum(TL_DATUM);
}

static void drawSosOverlayOnTftIfNeeded()
{
    // Прямой вывод в TFT после back.pushSprite() отключён.
    // Надпись SOS должна быть частью общего кадра, как блок текстового сообщения.
}
#if PROJECT_USE_GL050001C0_40
static int16_t clampToInt16(int value)
{
    if (value < -32768) return -32768;
    if (value > 32767) return 32767;
    return (int16_t)value;
}

static uint16_t clampToUint16(int value)
{
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return (uint16_t)value;
}

static GL050001C0_40_State buildGlDisplayState()
{
    updatePowerTelemetry();

    GL050001C0_40_State state = {};
    state.baseConnected = isBaseLinkActive();
    state.timeValid = GNSS_timeValid();
    state.gnssValid = GNSS_coordinatesValid();
    state.powerValid = g_powerTelemetryValid;
    state.sosActive = g_sosInputActive &&
                      (settings == nullptr || settings->display_sos != 0);
    state.showPowerVoltage = settings == nullptr || settings->power_voltage_view != 0;
    state.showPowerCurrent = settings == nullptr || settings->power_current_view != 0;
    state.showPowerBattery = settings == nullptr || settings->power_battery_view != 0;
    state.showLoraStatus = settings == nullptr || settings->rssi_view == VIEW_RSSI_ON;
    state.showGpsStatus = settings == nullptr || settings->gps_state_view != 0;
    state.hour = GNSS_hour();
    state.minute = GNSS_minute();
    state.satellites = GNSS_satellitesValid() ? GNSS_satellites() : 0;
    state.batteryPercent = g_powerBatteryPercent;
    state.voltageV = g_powerVoltageV;
    state.currentMa = g_powerCurrentMa;
    state.latitude = GNSS_latitude();
    state.longitude = GNSS_longitude();
    state.altitudeM = clampToInt16((int)lroundf(ThisAircraft.altitude));
    state.speedKmh = clampToUint16((int)lroundf(ThisAircraft.speed));
    state.courseDeg = (uint16_t)normalizeDisplayHeading360((int)lroundf(SystemDisplayCourseDeg()));
    state.loraTxPackets = Remote_loraTxPackets();
    state.loraRxPackets = Remote_loraRxPackets();
    state.loraRfHz = Remote_loraRfHz();
    state.loraRssiDb = clampToInt16(latestLoRaRssiValue());
    state.rs485TxPackets = RS485Display_txPackets();
    state.rs485RxPackets = RS485Display_rxPackets();

    for (int index = 0; index < MAX_TRACKING_OBJECTS &&
                        state.targetCount < GL050001C0_40_MAX_TARGETS; ++index)
    {
        if (Container[index].addr == 0) continue;

        updateTrafficPolarFromCoords(Container[index]);
        GL050001C0_40_Target& target = state.targets[state.targetCount++];
        target.address = Container[index].addr;
        memcpy(target.callsign, Container[index].callsign, 8);
        target.callsign[8] = '\0';
        target.squawk = clampToUint16(Container[index].squawk);
        target.altitudeM = clampToInt16((int)lroundf(Container[index].altitude));
        target.relativeAltitudeM = clampToInt16((int)lroundf(Container[index].altitude - ThisAircraft.altitude));
        target.speedKmh = clampToUint16((int)lroundf(Container[index].speed));
        target.courseDeg = (uint16_t)normalizeDisplayHeading360((int)lroundf(Container[index].course));
        target.latitude = Container[index].latitude;
        target.longitude = Container[index].longitude;
        target.signalSource = Container[index].signal_source;
        target.signalRssi = Container[index].signal_source == 2 ?
                            Container[index].rssi_rp2040 : Container[index].rssi_LoRa;
        target.distanceM = clampToUint16((int)lroundf(Container[index].distance));
        target.bearingDeg = (uint16_t)normalizeDisplayHeading360((int)lroundf(Container[index].bearing));
        target.verticalRate = clampToInt16(Container[index].vert_rate);

        const bool verticalValid = Container[index].altitude != 0.0f && ThisAircraft.altitude != 0.0f;
        const int verticalDistance = abs((int)target.relativeAltitudeM);
        target.alarmLevel = (uint8_t)radarAlertLevelForColor(
            radarAlertColorForTarget(target.distanceM, verticalDistance, verticalValid));
    }

    uint32_t nearestMeters = (uint32_t)kRadarScaleDefs[0].maxVisibleMeters;
    for (uint8_t index = 0; index < state.targetCount; ++index)
    {
        if (state.targets[index].distanceM > 0 &&
            state.targets[index].distanceM < nearestMeters)
        {
            nearestMeters = state.targets[index].distanceM;
        }
    }
    const uint8_t scaleIndex = effectiveRadarScaleIndex(nearestMeters);
    state.radarRangeM = (uint16_t)kRadarScaleDefs[scaleIndex].ringMeters;
    return state;
}
#endif

void Display_loop()
{
#if PROJECT_USE_GL050001C0_40
    if (!g_displayReady) return;
    updateExternalDisplayStatusFlags();
    GL050001C0_40_updateState(buildGlDisplayState());
    GL050001C0_40_loop();
    return;
#endif

    if (!g_displayReady || !back.created() || !backsprite.created())
    {
        return;
    }

    updateExternalDisplayStatusFlags();

    char buf[16];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
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
        angle = currentRadarRotationAngleDeg();

        /*Выполняем поворот по азимуту*/
        backsprite.pushRotated(&back, angle, TFT_BLACK);
        /***************    TFT_шкала дистанции    *******************/
        {
            FlyRfSpiGuard spiGuard(150);
            if (spiGuard)
            {
                drawSosOverlayIfNeeded();
                drawNoBaseConnectionOverlayIfNeeded();
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

        if (set_view_range != 0 && g_manualRangeActivatedMs != 0U)
        {
            if ((uint32_t)(millis() - g_manualRangeActivatedMs) >= BUTTON_OFF_DELAY)
            {
                clearManualRadarRangeOverride();
            }
        }

        if (new_buttton != 0)
        {
            switch (new_buttton)
            {
            case 1:
                (void)Tracker_confirmActiveTextMessage();
                break;
            case 2:
                switchManualRadarToSmallerRange();
                break;
            case 3:
                clearManualRadarRangeOverride();
                break;
            default:
                break;
            }

            new_buttton = 0;
        }



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
            thisAircraft_speed_tmr = (int)lroundf(ThisAircraft.speed); // Используем фактическую скорость собственного борта

            /*========== Курс собственного борта для поворота поля ================*/
            thisAircraft_course_tmr = normalizeDisplayHeading360((int)lroundf(SystemDisplayCourseDeg()));
            angle = currentRadarRotationAngleDeg();
            angle_old = angle;

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
                g_lastEffectiveRadarScaleIndex = defaultRangeIndex;
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
                    if (Container[i].addr && (displayUptimeSeconds() - Container[i].timestamp) <= TFT_EXPIRATION_TIME)  // Если есть самолет в базе и подошло время обновления данных
                    {
                        isTeam_all[i] = true;                                 // Сторонние самолеты определены и зарегтстрированы в базе

                        bool trafficPolarValid = updateTrafficPolarFromCoords(Container[i]);
                        if (!trafficPolarValid)
                        {
                            trafficPolarValid = (Container[i].distance > 0.0f || Container[i].bearing != 0.0f);
                        }

                        if (trafficPolarValid)
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

                // Запоминаем именно автоматический масштаб отдельно от временного ручного.
                // Первое двойное нажатие уменьшает текущий авто-диапазон,
                // а не переводит экран на максимальный диапазон.
                g_lastAutoRadarScaleIndex = autoRadarScaleIndex(min_distance);

                const uint8_t activeRangeIndex = effectiveRadarScaleIndex(min_distance);
                g_lastEffectiveRadarScaleIndex = activeRangeIndex;
                const RadarScaleDef& activeScale = kRadarScaleDefs[activeRangeIndex];
                divider = activeScale.ringMeters;
                divider_num = activeRangeIndex + 1;

                drawRadarBaseAndStatus();

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

                back.setTextFont(2);
                back.setTextSize(1);
                back.setTextDatum(TL_DATUM);

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

                            /* Отметку цели на TFT выводим строго в рассчитанной точке.
                               Перекрытие нескольких целей теперь допускается и не разносится искусственно. */
                            Container_alien_X[i] = new_x;  // Сохранить экранные координаты стороннего самолета без смещения
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

                            /* Определяем цвет точки и формуляра по опасному сближению именно для текущей цели. */
                            const uint32_t targetDistanceMeters = (uint32_t)lroundf(Container[i].distance);
                            const bool verticalValid = (alien_altitude_hysteresis[i] != 0 && thisAircraft_altitude_tmr != 0);
                            little_air_color[i] = radarAlertColorForTarget(targetDistanceMeters, VerticalSet, verticalValid);
                            Container[i].alarm_level = radarAlertLevelForColor(little_air_color[i]);
                            esp_task_wdt_reset();                            /*===============  Прямая отрисовка цели и формуляра в общий буфер ===============*/
                            const int labelAbsX = kRadarPivotX + Container_logbook_X[i];
                            const int labelAbsY = kRadarPivotY - Container_logbook_Y[i];
                            const int targetAbsX = kRadarPivotX + Container_alien_X[i];
                            const int targetAbsY = kRadarPivotY - Container_alien_Y[i];

                            if (Container[i].addr)
                            {
                                const int height_tmp = (int)round(height_difference[i] / 10);
                                char heightText[16];
                                formatSignedWithPlus(heightText, sizeof(heightText), height_tmp);
                                radarDrawLabelDirect(back, labelAbsX, labelAbsY, heightText, arrow_up_down[i], little_air_color[i], Air_txt_left[i]);
                            }

                            /* Записать скорость в формуляр движущегося самолета */
                            alien_speed_view[i] = 50 - ((int)Container[i].speed / 60 * 3);  // Расстояние стороннего самолета для вывода на дисплей

                            if (alien_speed_view[i] > 40)
                            {
                                alien_speed_view[i] = 40;
                            }

                            radarDrawTargetDirect(back, targetAbsX, targetAbsY, alient_course[i], alien_speed_view[i], little_air_color[i]);

                            if (placedSymbolCount < MAX_TRACKING_OBJECTS)
                            {
                                placedSymbols[placedSymbolCount].absX = (int16_t)targetAbsX;
                                placedSymbols[placedSymbolCount].absY = (int16_t)targetAbsY;
                                placedSymbols[placedSymbolCount].valid = true;
                                ++placedSymbolCount;
                            }
                            if (placedLabelCount < MAX_TRACKING_OBJECTS)
                            {
                                placedLabels[placedLabelCount].absX = (int16_t)labelAbsX;
                                placedLabels[placedLabelCount].absY = (int16_t)labelAbsY;
                                placedLabels[placedLabelCount].width = kRadarLabelSpriteWidth;
                                placedLabels[placedLabelCount].height = kRadarLabelSpriteHeight;
                                placedLabels[placedLabelCount].valid = true;
                                ++placedLabelCount;
                            }

                            esp_task_wdt_reset();

                        } //Закочить обработку данных самолетов с известными координатами

                        //============================= Конец обработки данных самолетов самолетов  с известными координатами ==========================
                    }
                }
            }
            else
            {
                drawRadarBaseAndStatus();
            }

            if (settings != nullptr && settings->display_set == INFO_DISPLAY_LORA_RAW)
            {
                drawRawLoRaPacketBlock();
            }
            drawTrackerTextMessageBlock();
            drawGnssStatusOverlay();
            drawSosOverlayIfNeeded();
            drawNoBaseConnectionOverlayIfNeeded();
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
