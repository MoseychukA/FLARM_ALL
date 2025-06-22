
#if defined(ESP32)

#include "sdkconfig.h"

#include <SPI.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <BLEDevice.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/efuse_reg.h>
#include <Wire.h>
#include <rom/rtc.h>
#include <rom/spi_flash.h>
#include <soc/adc_channel.h>
#include <flashchips.h>
#include <esp_task_wdt.h>
#include <driver/adc.h>


#include "SoC.h"
#include "TimeRF.h"
#include "EEPROMRF.h"
#include "RF.h"
#include "WiFiRF.h"
#include "Bluetooth.h"
#include "Baro.h"
#include "OLED.h"
#include "NMEA.h"
#include "D1090.h"

#include "Configuration_ESP32.h"
#include "ServiceMain.h"
#include "NotoSansMonoSCB20.h"
#include "NotoSansBold15.h"
#include "TrafficHelper.h"
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "Module1090.h"

#if defined(USE_TFT)
#include <TFT_eSPI.h>
#endif /* USE_TFT */

//#include <battery.h>

// SX12xx pin mapping
lmic_pinmap lmic_pins = {
    .nss  = SOC_GPIO_PIN_SS,
    .txe  = SOC_GPIO_PIN_LORA_TXE,
    .rxe  = SOC_GPIO_PIN_LORA_RXE,
    .rst  = SOC_GPIO_PIN_RST,
    .dio  = {LMIC_UNUSED_PIN, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
    .busy = SOC_GPIO_PIN_BUSY,//!!SOC_GPIO_PIN_TXE,
    .tcxo = LMIC_UNUSED_PIN,
};

const uint8_t whitening_pattern[] PROGMEM = { 0x05, 0xb4, 0x05, 0xae, 0x14, 0xda,
  0xbf, 0x83, 0xc4, 0x04, 0xb2, 0x04, 0xd6, 0x4d, 0x87, 0xe2, 0x01, 0xa3, 0x26,
  0xac, 0xbb, 0x63, 0xf1, 0x01, 0xca, 0x07, 0xbd, 0xaf, 0x60, 0xc8, 0x12, 0xed,
  0x04, 0xbc, 0xf6, 0x12, 0x2c, 0x01, 0xd9, 0x04, 0xb1, 0xd5, 0x03, 0xab, 0x06,
  0xcf, 0x08, 0xe6, 0xf2, 0x07, 0xd0, 0x12, 0xc2, 0x09, 0x34, 0x20 };

extern const uint8_t whitening_pattern[] PROGMEM;


WebServer server ( 80 );

#define WIDTH  320
#define HEIGHT 320


#if defined(USE_OLED)
U8X8_OLED_I2C_BUS_TYPE u8x8_heltec(OLED_PIN_RST);
#endif /* USE_OLED */

#if defined(USE_TFT)


// create a new sprite
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite back = TFT_eSprite(&tft);               // Спрайт фона
TFT_eSprite backsprite = TFT_eSprite(&tft);         // Спрайт отображения вращающегося поля воздушной обстановки
TFT_eSprite rows_Message = TFT_eSprite(&tft);       // Спрайт отображения текстов сообщений
TFT_eSprite location_Message = TFT_eSprite(&tft);   // Спрайт отображения текстов сообщений
TFT_eSprite state_SOS_button = TFT_eSprite(&tft);   // Спрайт отображения состояния кнопки SOS
TFT_eSprite DUMP1090_info = TFT_eSprite(&tft);      // Этот спрайт, площадка в котором будет располагатся информация о приеме данных с приемника 1090
TFT_eSprite time_info = TFT_eSprite(&tft);          // Этот спрайт, площадка в котором будет располагатся информация о времени с GPS

TFT_eSprite* arrow[MAX_TRACKING_OBJECTS];           // Спрайт отображения стрелки
TFT_eSprite* Air_txt_Sprite[MAX_TRACKING_OBJECTS];  // Этот спрайт, площадка в котором будет располагатся формуляр стороннего самолета
TFT_eSprite* airplane[MAX_TRACKING_OBJECTS];        // Этот спрайт, площадка в котором будет располагатся изображение стороннего самолета DUMP1090
TFT_eSprite* area_airplane[MAX_TRACKING_OBJECTS];   // Этот спрайт, площадка в котором будет располагатся спрайт airplane стороннего самолета
TFT_eSprite power1 = TFT_eSprite(&tft);             // Спрайт отображения заряда аккумулятора 

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
int Container_arrow_X[MAX_TRACKING_OBJECTS];        // Координаты стрелки стороннего самолета
int Container_arrow_Y[MAX_TRACKING_OBJECTS];        // Координаты стрелки стороннего самолета
uint8_t arrow_up_down[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
uint8_t arrow_up_down_old[MAX_TRACKING_OBJECTS];    // флаг стрелки вверх или вниз
uint8_t DUMP1090_arrow_up_down[MAX_TRACKING_OBJECTS];        // флаг стрелки вверх или вниз
uint8_t DUMP1090_arrow_up_down_old[MAX_TRACKING_OBJECTS];    // флаг стрелки вверх или вниз
bool Air_txt_left[MAX_TRACKING_OBJECTS];            // флаг расположения формуляра слева или справа

word  little_air_color[MAX_TRACKING_OBJECTS];       // Цвет предупреждения столкновения с сторонним самолетом 

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

int dump1090_speed[MAX_TRACKING_OBJECTS];                          // 
String dump1090_info_txt[MAX_TRACKING_OBJECTS];                    // 

bool isTeam_all[MAX_TRACKING_OBJECTS] = { false };                 // Удалить данные по самолету

//......................................colors
#define backColor  0x0026

//===================================================================================
int cx = 160;
int cy = 160;
int rx = 158;
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

bool text_call = false;
char msg_mem_tmp[Number_of_bytes_block] = "";

int thisAircraft_altitude_tmr = 0;       // ThisAircraft
int thisAircraft_speed_tmr = 0;
int thisAircraft_course_tmr = 0;

uint8_t index_nearest_aircraft = 0;      // индекс ближайшего самолета

int view_alien_count = 0;                // Переменная для определения количества сторонних самолетов.

int16_t alient_course0 = 0;              // Курс ближайшего стороннего самолета
int16_t alient_speed0  = 0;              // Скорость ближайшего стороннего самолета
int8_t  txt_loc_speed  = 87;             // место вывода текста скорости стороннегосамолета

int32_t divider = 2000;                  //делитель равен половине полной шкалы
int divider_num = 1;
uint16_t x_cont;
uint16_t y_cont;
uint16_t radar_x = 0;
uint16_t radar_y = 0;                    //(tft_radar->width() - tft_radar->height()) / 2;
uint16_t radar_w = 320;                  //tft->width();

uint16_t radar_center_x = radar_w / 2;
uint16_t radar_center_y = radar_y + radar_w / 2;
uint16_t radius = radar_w / 2 - 2;

int16_t rel_x;
int16_t rel_y;
int16_t new_rel_x;
int16_t new_rel_y;
int16_t new_form_x;
int16_t new_form_y;

int16_t new_x;
int16_t new_y;

int16_t form_x = 0;
int16_t form_y = 0;
int16_t form_arrow_x = 0;
int16_t form_arrow_y = 0;

int test_curse = 0;
float Aircraft_latitude_old1 = 0;
float Aircraft_longitude_old1 = 0;
unsigned long previousMillis_msg = 0;        // will store last time was updated
unsigned long previousMillis_SOS = 0;        // will store last time SOS was updated
bool SOS_Sprite_on_off;                      // Режим отображения спрайта включен или потушен
bool SOS_View_on_off;                        // Флаг отображения включен или потушен
bool new_SOS_flag;                           // Флаг наличия импульсов SOS
bool flashing_on_off;                        // Режим отображения сообщения

uint8_t  fix_tmp = false;

bool TFT_LED_MESSAGE = false;                // Управление подсветкой дисплея при приходе сообщения
bool TFT_LED_BASE    = false;                // Управление подсветкой дисплея при налмчии информации в базе
bool TFT_LED_GPS     = false;                // Управление подсветкой дисплея при наличии определения координат
bool TFT_LED_TEST    = false;                // Управление подсветкой дисплея в тестовом режиме
//====================================================================================
#endif /* USE_TFT */

extern CommandHandlerClass CommandHandler;

bool setMessageRead = false;
int set_view_range = 0;

static int esp32_board = ESP32_DEVKIT; /* default */
static size_t ESP32_Min_AppPart_Size = 0;

static bool GPIO_21_22_are_busy = false;

static union {
  uint8_t efuse_mac[6];
  uint64_t chipmacid;
};

static bool TFT_display_frontpage       = false;
static uint32_t prev_tx_packets_counter = 0;
static uint32_t prev_rx_packets_counter = 0;
extern uint32_t tx_packets_counter, rx_packets_counter;
extern bool loopTaskWDTEnabled;

const char *ESP32SX_Device_Manufacturer = FLYRF_IDENT;
const char *ESP32SX_Model_Stand  = "Standalone Edition"; /* 303a:8132 */
const char *ESP32S3_Model_Prime3 = "Prime Edition Mk.3"; /* 303a:8133 */
const char *ESP32S3_Model_Ham    = "Ham Edition";        /* 303a:818F */
const char *ESP32S3_Model_Midi   = "Midi Edition";       /* 303a:81A0 */
const uint16_t ESP32SX_Device_Version = FLYRF_USB_FW_VERSION;


#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include <Adafruit_SPIFlash.h>
#include "uCDB.hpp"

enum {
  EXTERNAL_FLASH_DEVICE_COUNT
};


#include "soc/rtc.h"
static uint32_t calibrate_one(rtc_cal_sel_t cal_clk, const char *name)
{
    const uint32_t cal_count = 1000;
    const float factor = (1 << 19) * 1000.0f;
    uint32_t cali_val;
    for (int i = 0; i < 5; ++i) 
    {
        cali_val = rtc_clk_cal(cal_clk, cal_count);
    }
    return cali_val;
}

#define CALIBRATE_ONE(cali_clk) calibrate_one(cali_clk, #cali_clk)


#endif /* CONFIG_IDF_TARGET_ESP32S3 */

//#if defined(ENABLE_D1090_INPUT)
//#include <mode-s.h>
//
//mode_s_t state;
//#endif /* ENABLE_D1090_INPUT */



static uint32_t ESP32_getFlashId()
{
  return g_rom_flashchip.device_id;
}

#if defined(CORE_DEBUG_LEVEL) && CORE_DEBUG_LEVEL>0 && !defined(TAG)
#define TAG "MAC"
#endif



static void ESP32_setup()
{
#if !defined(FLYRF_ADDRESS)  // Если не указан индивидуальный адрес

  esp_err_t ret = ESP_OK;
  uint8_t null_mac[6] = {0};
  pinMode(SOC_GPIO_PIN_LED, OUTPUT);
  digitalWrite(SOC_GPIO_PIN_LED, HIGH);

  pinMode(SOC_GPIO_PIN_TFT_LED, OUTPUT);
  digitalWrite(SOC_GPIO_PIN_TFT_LED, HIGH);

  ret = esp_efuse_mac_get_custom(efuse_mac);
  if (ret != ESP_OK) 
  {
      ESP_LOGE(TAG, "Get base MAC address from BLK3 of EFUSE error (%s)", esp_err_to_name(ret));
    /* If get custom base MAC address error, the application developer can decide what to do:
     * abort or use the default base MAC address which is stored in BLK0 of EFUSE by doing
     * nothing.
     */

    ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
    chipmacid = ESP.getEfuseMac();
  }
  else 
  {
    if (memcmp(efuse_mac, null_mac, 6) == 0)
    {
      ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
      chipmacid = ESP.getEfuseMac();
    }
  }
#endif /* FLYRF_ADDRESS */

  size_t flash_size = spi_flash_get_chip_size();
  size_t min_app_size = flash_size;

  esp_partition_iterator_t it;
  const esp_partition_t *part;

  it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
  if (it) 
  {
    do 
    {
      part = esp_partition_get(it);
      if (part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) 
      {
        continue;
      }
      if (part->size < min_app_size) 
      {
        min_app_size = part->size;
      }
    } while (it = esp_partition_next(it));

    if (it) esp_partition_iterator_release(it);
  }

  if (min_app_size && (min_app_size != flash_size)) 
  {
    ESP32_Min_AppPart_Size = min_app_size;
  }

  if (psramFound()) 
  {

    uint32_t flash_id = ESP32_getFlashId();

     esp32_board = ESP32_S3_DEVKIT;
  }
  else 
  {
      esp32_board    = ESP32_S3_DEVKIT;
  }

  Serial.begin(SERIAL_OUT_BR, SERIAL_OUT_BITS);
  Serial2.begin(SERIAL_TRACKER_SPEED, SERIAL_IN_BITS, SOC_GPIO_PIN_TRACKER_RX, SOC_GPIO_PIN_TRACKER_TX);

}

static void ESP32_post_init()
{
 
    Serial.println();
    Serial.println(F("Power-on Self Test"));
    Serial.println();
    Serial.flush();

    Serial.println(F("Built-in components:"));

    Serial.print(F("RADIO    : "));
    Serial.println(hw_info.rf      == RF_IC_SX1262 || hw_info.rf      == RF_IC_SX1276     ? F("PASS") : F("FAIL"));
    Serial.flush();
    Serial.print(F("GNSS     : "));
    Serial.println(hw_info.gnss    != GNSS_MODULE_NONE ? F("PASS") : F("FAIL"));
    Serial.flush();
    Serial.print(F("DISPLAY  : "));
    Serial.println(hw_info.display == DISPLAY_OLED ? F("PASS") : F("FAIL"));
    Serial.flush();
    Serial.print(F("BARO     : "));
    Serial.println(hw_info.baro  == BARO_MODULE_BMP180 ? F("PASS") : F("N/A"));
    Serial.flush();

    Serial.println();
    Serial.println(F("Power-on Self Test is complete."));
    Serial.flush();
  
    Serial.println();
    Serial.println(F("Data output device(s):"));

    Serial.print(F("NMEA   - "));
    switch (settings->nmea_out)
    {
        case NMEA_UART       :  Serial.println(F("UART"));      break;
        case NMEA_USB        :  Serial.println(F("USB CDC"));   break;
        case NMEA_UDP        :  Serial.println(F("UDP"));       break;
        case NMEA_TCP        :  Serial.println(F("TCP"));       break;
        case NMEA_BLUETOOTH  :  Serial.println(F("Bluetooth")); break;
        case NMEA_OFF        :
        default              :  Serial.println(F("NULL"));      break;
    }

    Serial.print(F("D1090  - "));
    switch (settings->d1090)
    {
        case D1090_UART      :  Serial.println(F("UART"));      break;
        case D1090_USB       :  Serial.println(F("USB CDC"));   break;
        case D1090_BLUETOOTH :  Serial.println(F("Bluetooth")); break;
        case D1090_OFF       :
        default              :  Serial.println(F("NULL"));      break;
    }

    Serial.println();
    Serial.flush();


    #if defined(USE_OLED)
      OLED_info1();
    #endif /* USE_OLED */
 
}

static void ESP32_loop()
{
  bool is_irq = false;
  bool down = false;
}

static void ESP32_fini(int reason)
{
  esp_deep_sleep_start();
}

static void ESP32_reset()
{
  ESP.restart();
}

static uint32_t ESP32_getChipId()
{
#if !defined(FLYRF_ADDRESS)
  uint32_t id = (uint32_t) efuse_mac[5]        | ((uint32_t) efuse_mac[4] << 8) | \
               ((uint32_t) efuse_mac[3] << 16) | ((uint32_t) efuse_mac[2] << 24);

  return DevID_Mapper(id);
#else
  return (FLYRF_ADDRESS & 0xFFFFFFFFU );
#endif /* FLYRF_ADDRESS */
}

static struct rst_info reset_info = {
  .reason = REASON_DEFAULT_RST,
};

static void* ESP32_getResetInfoPtr()
{
  switch (rtc_get_reset_reason(0))
  {
    case POWERON_RESET          : reset_info.reason = REASON_DEFAULT_RST; break;
    case DEEPSLEEP_RESET        : reset_info.reason = REASON_DEEP_SLEEP_AWAKE; break;
    case TG0WDT_SYS_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case TG1WDT_SYS_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case RTCWDT_SYS_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case INTRUSION_RESET        : reset_info.reason = REASON_EXCEPTION_RST; break;
    case RTCWDT_CPU_RESET       : reset_info.reason = REASON_WDT_RST; break;
    case RTCWDT_BROWN_OUT_RESET : reset_info.reason = REASON_EXT_SYS_RST; break;
    case RTCWDT_RTC_RESET       :
      /* Slow start of GD25LQ32 causes one read fault at boot time with current ESP-IDF */
      if (ESP32_getFlashId() == MakeFlashId(GIGADEVICE_ID, GIGADEVICE_GD25LQ32))
                                  reset_info.reason = REASON_DEFAULT_RST;
      else
                                  reset_info.reason = REASON_WDT_RST;
                                  break;
#if defined(CONFIG_IDF_TARGET_ESP32)
    case SW_RESET               : reset_info.reason = REASON_SOFT_RESTART; break;
    case OWDT_RESET             : reset_info.reason = REASON_WDT_RST; break;
    case SDIO_RESET             : reset_info.reason = REASON_EXCEPTION_RST; break;
    case TGWDT_CPU_RESET        : reset_info.reason = REASON_WDT_RST; break;
    case SW_CPU_RESET           : reset_info.reason = REASON_SOFT_RESTART; break;
    case EXT_CPU_RESET          : reset_info.reason = REASON_EXT_SYS_RST; break;
#endif /* CONFIG_IDF_TARGET_ESP32 */
    default                     : reset_info.reason = REASON_DEFAULT_RST;
  }

  return (void *) &reset_info;
}

static String ESP32_getResetInfo()
{
  switch (rtc_get_reset_reason(0))
  {
    case POWERON_RESET          : return F("Vbat power on reset");
    case DEEPSLEEP_RESET        : return F("Deep Sleep reset digital core");
    case TG0WDT_SYS_RESET       : return F("Timer Group0 Watch dog reset digital core");
    case TG1WDT_SYS_RESET       : return F("Timer Group1 Watch dog reset digital core");
    case RTCWDT_SYS_RESET       : return F("RTC Watch dog Reset digital core");
    case INTRUSION_RESET        : return F("Instrusion tested to reset CPU");
    case RTCWDT_CPU_RESET       : return F("RTC Watch dog Reset CPU");
    case RTCWDT_BROWN_OUT_RESET : return F("Reset when the vdd voltage is not stable");
    case RTCWDT_RTC_RESET       : return F("RTC Watch dog reset digital core and rtc module");
    default                     : return F("No reset information available");
  }
}

static String ESP32_getResetReason()
{

  switch (rtc_get_reset_reason(0))
  {
    case POWERON_RESET          : return F("POWERON_RESET");
    case DEEPSLEEP_RESET        : return F("DEEPSLEEP_RESET");
    case TG0WDT_SYS_RESET       : return F("TG0WDT_SYS_RESET");
    case TG1WDT_SYS_RESET       : return F("TG1WDT_SYS_RESET");
    case RTCWDT_SYS_RESET       : return F("RTCWDT_SYS_RESET");
    case INTRUSION_RESET        : return F("INTRUSION_RESET");
    case RTCWDT_CPU_RESET       : return F("RTCWDT_CPU_RESET");
    case RTCWDT_BROWN_OUT_RESET : return F("RTCWDT_BROWN_OUT_RESET");
    case RTCWDT_RTC_RESET       : return F("RTCWDT_RTC_RESET");
    default                     : return F("NO_MEAN");
  }
}

static uint32_t ESP32_getFreeHeap()
{
  return ESP.getFreeHeap();
}

static long ESP32_random(long howsmall, long howBig)
{
  return random(howsmall, howBig);
}

static uint32_t ESP32_maxSketchSpace()
{
  return ESP32_Min_AppPart_Size ? ESP32_Min_AppPart_Size :
           SoC->id == SOC_ESP32S3 ?
             0x200000  /* 8MB-tinyuf2.csv */ :
             0x1E0000; /* min_spiffs.csv */
}

static const int8_t ESP32_dBm_to_power_level[21] = {
  8,  /* 2    dBm, #0 */
  8,  /* 2    dBm, #1 */
  8,  /* 2    dBm, #2 */
  8,  /* 2    dBm, #3 */
  8,  /* 2    dBm, #4 */
  20, /* 5    dBm, #5 */
  20, /* 5    dBm, #6 */
  28, /* 7    dBm, #7 */
  28, /* 7    dBm, #8 */
  34, /* 8.5  dBm, #9 */
  34, /* 8.5  dBm, #10 */
  44, /* 11   dBm, #11 */
  44, /* 11   dBm, #12 */
  52, /* 13   dBm, #13 */
  52, /* 13   dBm, #14 */
  60, /* 15   dBm, #15 */
  60, /* 15   dBm, #16 */
  68, /* 17   dBm, #17 */
  74, /* 18.5 dBm, #18 */
  76, /* 19   dBm, #19 */
  78  /* 19.5 dBm, #20 */
};

static void ESP32_WiFi_set_param(int ndx, int value)
{
#if !defined(EXCLUDE_WIFI)
  uint32_t lt = value * 60; /* in minutes */

  switch (ndx)
  {
  case WIFI_PARAM_TX_POWER:
    if (value > 20) {
      value = 20; /* dBm */
    }

    if (value < 0) {
      value = 0; /* dBm */
    }

    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(ESP32_dBm_to_power_level[value]));
    break;
  case WIFI_PARAM_DHCP_LEASE_TIME:
    tcpip_adapter_dhcps_option(
      (tcpip_adapter_dhcp_option_mode_t) TCPIP_ADAPTER_OP_SET,
      (tcpip_adapter_dhcp_option_id_t)   TCPIP_ADAPTER_IP_ADDRESS_LEASE_TIME,
      (void*) &lt, sizeof(lt));
    break;
  default:
    break;
  }
#endif /* EXCLUDE_WIFI */
}

static IPAddress ESP32_WiFi_get_broadcast()
{
  tcpip_adapter_ip_info_t info;
  IPAddress broadcastIp;

  if (WiFi.getMode() == WIFI_STA) 
  {
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &info);
  }
  else 
  {
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &info);
  }
  broadcastIp = ~info.netmask.addr | info.ip.addr;

  return broadcastIp;
}

static void ESP32_WiFi_transmit_UDP(int port, byte *buf, size_t size)
{
#if !defined(EXCLUDE_WIFI)
  IPAddress ClientIP;
  WiFiMode_t mode = WiFi.getMode();
  int i = 0;

  switch (mode)
  {
  case WIFI_STA:
    ClientIP = ESP32_WiFi_get_broadcast();

    Uni_Udp.beginPacket(ClientIP, port);
    Uni_Udp.write(buf, size);
    Uni_Udp.endPacket();

    break;
  case WIFI_AP:
    wifi_sta_list_t stations;
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&stations));

    tcpip_adapter_sta_list_t infoList;
    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stations, &infoList));

    while(i < infoList.num) {
      ClientIP = infoList.sta[i++].ip.addr;

      Uni_Udp.beginPacket(ClientIP, port);
      Uni_Udp.write(buf, size);
      Uni_Udp.endPacket();
    }
    break;
  case WIFI_OFF:
  default:
    break;
  }
#endif /* EXCLUDE_WIFI */
}

static void ESP32_WiFiUDP_stopAll()
{
/* not implemented yet */
}

static bool ESP32_WiFi_hostname(String aHostname)
{
#if defined(EXCLUDE_WIFI)
  return false;
#else
  return WiFi.setHostname(aHostname.c_str());
#endif /* EXCLUDE_WIFI */
}

static int ESP32_WiFi_clients_count()
{
#if defined(EXCLUDE_WIFI)
  return 0;
#else
  WiFiMode_t mode = WiFi.getMode();

  switch (mode)
  {
  case WIFI_AP:
    wifi_sta_list_t stations;
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&stations));

    tcpip_adapter_sta_list_t infoList;
    ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stations, &infoList));

    return infoList.num;
  case WIFI_STA:
  default:
    return -1; /* error */
  }
#endif /* EXCLUDE_WIFI */
}

static bool ESP32_EEPROM_begin(size_t size)
{
  bool rval = true;

#if !defined(EXCLUDE_EEPROM)
  rval = EEPROM.begin(size);
#endif

  return rval;
}

static void ESP32_EEPROM_extension(int cmd)
{
  if (cmd == EEPROM_EXT_LOAD) {
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(USE_USB_HOST)
    if (settings->nmea_out == NMEA_USB) {
      settings->nmea_out = NMEA_UART;
    }
    if (settings->d1090 == D1090_USB) {
      settings->d1090 = D1090_UART;
    }
#endif /* CONFIG_IDF_TARGET_ESP32 */
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) ||  defined(CONFIG_IDF_TARGET_ESP32C3)
    if (settings->bluetooth != BLUETOOTH_NONE) 
    {
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
      settings->bluetooth = BLUETOOTH_LE_HM10_SERIAL;
#endif /* CONFIG_IDF_TARGET_ESP32S3 || C3 */
    }

#endif /* CONFIG_IDF_TARGET_ESP32S2 || S3 || C3 */

    /* AUTO and UK RF bands are deprecated since Release v1.3 */
    if (settings->band == RF_BAND_AUTO || settings->band == RF_BAND_UK) {
      settings->band = RF_BAND_EU;
    }
  }
}

static void ESP32_SPI_begin()
{
    SPI.begin(SOC_GPIO_PIN_SCK, SOC_GPIO_PIN_MISO,
        SOC_GPIO_PIN_MOSI, SOC_GPIO_PIN_SS);
}

static void ESP32_swSer_begin(unsigned long baud)
{

    /* open Standalone's GNSS port */
    Serial_GNSS_In.begin(baud, SERIAL_IN_BITS, SOC_GPIO_PIN_GNSS_RX, SOC_GPIO_PIN_GNSS_TX);
    /* Default Rx buffer size (256 bytes) is sometimes not big enough */
    Serial_GNSS_In.setRxBufferSize(512);

    /* Need to gather some statistics on variety of flash IC usage */
    Serial.print(F("Flash memory ID: "));
    Serial.println(ESP32_getFlashId(), HEX);
}

static void ESP32_swSer_enableRx(boolean arg)
{

}

#if defined(USE_OLED)
static byte ESP32_OLED_ident(TwoWire *bus)
{
  uint8_t r = 0;
  byte rval = DISPLAY_OLED;

  bus->beginTransmission(SSD1306_OLED_I2C_ADDR);
  bus->write(0x00);
  bus->endTransmission();
  bus->requestFrom((int) SSD1306_OLED_I2C_ADDR, 1);
  if (bus->available()) 
  {
    r = bus->read();
    r &= 0x0f;
    if (r == 0x08 || r == 0x00 || r == 0x0C) 
    {
        rval = DISPLAY_OLED; // SSD1306
    } 
  }

#if 1
  Serial.print("INFO: OLED subtype ");
  Serial.println(r, HEX);
#endif

  return rval;
}
#endif /* USE_OLED */

static byte ESP32_Display_setup()
{
  byte rval = DISPLAY_NONE;


  if (settings->akk_view == VIEW_AKK_ON)
  {
      /*Настроить измерение аккумулятора*/
      adc1_config_width(ADC_WIDTH_12Bit);
      adc1_config_channel_atten(ADC1_CHANNEL_2, ADC_ATTEN_11db); //set reference voltage to internal
  }
  
#if defined(USE_OLED)
    bool has_oled = false;

    Wire.begin(OLED_PIN_SDA, OLED_PIN_SCL);
    Wire.beginTransmission(SSD1306_OLED_I2C_ADDR);
    has_oled = (Wire.endTransmission() == 0);
    WIRE_FINI(Wire);
    if (has_oled)
    {
        u8x8 = &u8x8_heltec;
        esp32_board = ESP32_S3_DEVKIT;
        rval = DISPLAY_OLED;
    }

  
    if (u8x8) 
    {
      u8x8->begin();
      u8x8->setFlipMode(OLED_flip);
      u8x8->setFont(u8x8_font_chroma48medium8_r);
      u8x8->clear();

      uint8_t shift_y = 0;
      u8x8->draw2x2String( 2, 2 - shift_y, SoftRF_text1);

      if (shift_y) 
      {
        u8x8->drawString   ( 6, 3, SoftRF_text2);
        u8x8->draw2x2String( 2, 4, SoftRF_text3);
      }

      u8x8->drawString   ( 3, 6 + shift_y,   FLYRF_FIRMWARE_VERSION);
      u8x8->drawString   (11, 6 + shift_y, ISO3166_CC[settings->band]);
    }

#endif /* USE_OLED */

 
#if defined(USE_TFT)

  //  tft = new TFT_eSPI();
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_NAVY);

    uint16_t tbw1;
    uint16_t x_tft, y_tft;

    const char EPD_SoftRF_text1[] = "FlyRF";
    const char EPD_SoftRF_text2[] = "www.decima.ru";
    const char EPD_SoftRF_text3[] = "DECIMA";
    const char EPD_SoftRF_text6[] = "(C) 2024";

    tft.fillScreen(TFT_NAVY);

    tft.setTextColor(TFT_WHITE); //TFT_WHITE TFT_BLACK
    tft.setTextWrap(false);

    tft.setFreeFont(&FreeMonoBold24pt7b);

    x_tft = 90;
    y_tft = 80;
    tft.setCursor(x_tft, y_tft);
    tft.print(EPD_SoftRF_text1);

    x_tft = 80;
    y_tft = 150;
    tft.setCursor(x_tft, y_tft);
    tft.print(EPD_SoftRF_text3);

    tft.setFreeFont(&FreeSerif9pt7b);

    x_tft = 10;
    y_tft = 205;
    tft.setCursor(x_tft, y_tft);
    tft.print(EPD_SoftRF_text2);

    x_tft = 10;
    y_tft = tft.height() - tft.fontHeight() + 10;
    tft.setCursor(x_tft, y_tft);
    tft.print(EPD_SoftRF_text6);

    String Current_version = service.getVer();
    tbw1 = tft.textWidth(Current_version);
    x_tft = (tft.width() - tbw1) - 4;
    y_tft = tft.height() - tft.fontHeight() + 10;
    tft.setCursor(x_tft, y_tft);
    tft.print(Current_version);
   
    back.createSprite(320, 320);
    back.setColorDepth(8);


    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        Air_txt_Sprite[i] = new TFT_eSprite(&tft);     // Спрайт информации стороннего воздушного объекта
        Air_txt_Sprite[i]->createSprite(55, 15);
        Air_txt_Sprite[i]->setPivot(27, 7);
 
        arrow[i] = new TFT_eSprite(&tft);              // Спрайт информации стороннего воздушного объекта
        arrow[i]->createSprite(10, 10);                // Спрайт отображения стрелка вверх/вниз
 
        airplane[i] = new TFT_eSprite(&tft);          // Спрайт информации стороннего воздушного объекта
        airplane[i]->createSprite(100, 100);          // Спрайт отображения объекта, полученного из DUMP1090. 
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
        esp_task_wdt_reset();
    }

    DUMP1090_info.createSprite(30, 30);
    DUMP1090_info.setPivot(15, 15);
    DUMP1090_info.fillSprite(TFT_BLACK);      // Закрасим поле самолетика

    power1.createSprite(74, 20);

    time_info.createSprite(60, 25);

    backsprite.createSprite(320, 320);
    backsprite.loadFont(NotoSansMonoSCB20);          // Загружаем шрифты символов направления света
    backsprite.setSwapBytes(true);
    backsprite.setTextColor(TFT_WHITE, TFT_BLACK);
    backsprite.setTextDatum(4);
    backsprite.setPivot(160, 160);                   // Назначаем центр вращения спрайта воздушной обстановки

    int a = 270;
    for (int i = 0; i < 360; i++)
    {
        fx[i] = ((rx - 5) * cos(rad * a)) + cx;    //Длина линии внешняя точка
        fy[i] = ((rx - 5) * sin(rad * a)) + cy;    //Длина линии внешняя точка
        px[i] = ((rx - 14) * cos(rad * a)) + cx;  //Длина линии внутрення точка
        py[i] = ((rx - 14) * sin(rad * a)) + cy;  //Длина линии внешняя точка
        px[i] = ((rx - 14) * cos(rad * a)) + cx;  //Длина линии внешняя точка
        py[i] = ((rx - 14) * sin(rad * a)) + cy;  //Длина линии внутрення точка
        px1[i] = ((rx - 5) * cos(rad * a)) + cx;  //Длина линии внутрення точка
        py1[i] = ((rx - 5) * sin(rad * a)) + cy;  //Длина линии внутрення точка
        lx[i] = ((rx - 6) * cos(rad * a)) + cx;   //Положение символов по кругу
        ly[i] = ((rx - 6) * sin(rad * a)) + cy;   //Положение символов по кругу
        nx[i] = ((rx - 36) * cos(rad * a)) + cx;
        ny[i] = ((rx - 36) * sin(rad * a)) + cy;

        a++;
        if (a == 360)
            a = 0;
    }

    if (settings->sos_view == VIEW_SOS_ON)
    {
        pinMode(SOC_GPIO_PIN_SOS, INPUT);        // устанавливает режим работы
    }


#endif /* USE_TFT */

  return rval;
}

#if defined(USE_TFT)
//----------------------------------------------------------------------------------------------------------
static void Draw_circular_scale()
{

    /* Рисуем круглую шкалу серым цветом и символы сторон света белым*/
    for (int i = 0; i < 36; i++)
    {
        // unsigned short color2 = TFT_DARKGREY;
        if (i % 3 == 0)
        {
            backsprite.drawWedgeLine(fx[i * 10], fy[i * 10], px[i * 10], py[i * 10], 1, 1, TFT_DARKGREY);
            backsprite.setTextColor(TFT_DARKGREY, TFT_BLACK);
            if (i == 0)
            {
                backsprite.drawString("N", lx[i * 10] + 1, ly[i * 10], TFT_DARKGREY);
            }
            if (i == 9)
            {
                backsprite.drawString("E", lx[i * 10], ly[i * 10], TFT_DARKGREY);
            }
            if (i == 18)
            {
                backsprite.drawString("S", lx[i * 10], ly[i * 10], TFT_DARKGREY);
            }
            if (i == 27)
            {
                backsprite.drawString("W", lx[i * 10], ly[i * 10], TFT_DARKGREY);
            }
        }
        else
        {
            backsprite.drawWedgeLine(fx[i * 10], fy[i * 10], px1[i * 10], py1[i * 10], 1, 1, TFT_DARKGREY);
        }
    }
    esp_task_wdt_reset();

    /*Рисуем малый серый круг*/
    backsprite.drawCircle(cx, 160, 80, TFT_DARKGREY);

}

static void waiting_txt() // Вывод текста ожидания определения координат
{
    location_Message.createSprite(202, 95);
    location_Message.fillSprite(TFT_BLACK);
    location_Message.setTextColor(TFT_YELLOW, TFT_BLACK);
    location_Message.setTextDatum(CC_DATUM);
    location_Message.setFreeFont(&FreeSerif12pt7b);
    location_Message.drawString("LOCATION", 101, 10);
    location_Message.drawString("DETERMINATION", 101, 44);
    location_Message.drawString("WAIT...", 101, 74);
    location_Message.pushToSprite(&back, 70, 70, TFT_BLACK);
}


static void drawMessage()
{
    char msg_mem[Number_of_bytes_block] = "";
    char time_msg[Number_of_bytes_time] = "";
    //Serial2.println(msg_mem);
    /*Вывести на дисплей сообщение*/

   // strncpy(msg_mem, CommandHandler.msg_tmp_all, strlen(CommandHandler.msg_tmp_all));
    strncpy(msg_mem, msg_mem_tmp, strlen(msg_mem_tmp));

    rows_Message.createSprite(320, 52);
    rows_Message.fillSprite(TFT_BLACK);
    rows_Message.setTextColor(TFT_YELLOW, backColor);
    rows_Message.setTextDatum(TL_DATUM);
    rows_Message.setTextSize(2);

    // Необходимо вычислить количество символов в каждой строке.

    int lette_num = mb_strlen(msg_mem, 26);  // Определяем реальное количество байт 26 символов в первой строчке

    char str1[27] = { 0 };
    strncpy(str1, msg_mem, lette_num);      // Сформировали первую строчку  из 26 символов
    str1[lette_num] = 0;                    // записать 0 в конес первой строчки
    rows_Message.drawString(str1, 0, 0);    // Запишем первую строчку в спрайт

    char str_tmp[54] = { 0 };               // Временный массив для второй строчки
    strcpy(str_tmp, msg_mem + lette_num);   // Записать в str_tmp текст начиная со второй строчки
    lette_num = mb_strlen(str_tmp, 26);     // Определяем реальное количество байт 27 символов во второй строчке
    char str2[27] = { 0 };                  // Назначить массив для второй строчки
    strncpy(str2, str_tmp, lette_num);      // Копируем вторую строчку с ограничением  
    str2[lette_num] = 0;                    // записать 0 в конес второй строчки
    rows_Message.drawString(str2, 0, 17);   // Запишем вторую строчку в спрайт

    char str_tmp1[80] = { 0 };
    lette_num = mb_strlen(msg_mem, 52);     // Ищем конец второй строчки
    strcpy(str_tmp1, msg_mem + lette_num);  // Копируем текст третьей строчки

    lette_num = mb_strlen(str_tmp1, 26);    // Ищем конец третьей строчки
    char str3[26] = { 0 };

    strncpy(str3, str_tmp1, lette_num);
    str3[lette_num] = 0;
    rows_Message.drawString(str3, 0, 35);  // Запишем третью строчку в спрайт

    rows_Message.pushToSprite(&back, 0, 25, TFT_BLACK);
    back.pushSprite(0, 0);
    rows_Message.deleteSprite();
}

static void display_state_SOS_button()
{
    if (!state_SOS_button.created())
    {
        state_SOS_button.createSprite(86, 50);
        state_SOS_button.fillSprite(TFT_BLACK);
        state_SOS_button.setTextColor(TFT_RED, backColor);
        state_SOS_button.setTextDatum(CC_DATUM);
        state_SOS_button.setFreeFont(&FreeSerif24pt7b);
    }
}

static void execution_state_SOS_button()
{
    if (settings->sos_view == VIEW_SOS_ON)
    {
        /* Проверить пришло ли новое состояние кнопки SOS. */
        new_SOS_flag = digitalRead(SOC_GPIO_PIN_SOS);                               // Получить признак состояния кнопки SOS
  
        static uint32_t tmr_flashing_SOS = millis(); 

        if (new_SOS_flag)
        {
            tmr_flashing_SOS = millis();
            if (SOS_View_on_off == false)
            {
                SOS_View_on_off = true;
            }
        }

        if ((millis() - tmr_flashing_SOS > SOS_FLASHING_ON)&&(SOS_View_on_off == true))
        {
            tmr_flashing_SOS = millis();
            SOS_View_on_off = false;
        } 
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 // Функция подсчёта количества символов в строке utf8,
// состоящей из букв английского и русского алфавитов, цифр, общепринятых символов...

int mb_strlen(char* source, int letter_n)  // как в php :)
{
    int i, k;
    int target = 0;
    unsigned char n;
    char m[2] = { '0', '\0' };
    k = strlen(source);
    i = 0;

    while (i < k) {
        n = source[i]; i++;

        if (n >= 0xBF)
        {
            switch (n)
            { 
            case 0xD0:
            {
                n = source[i]; i++;
                if (n == 0x81) { n = 0xA8; break; }
                if (n >= 0x90 && n <= 0xBF) n = n + 0x2F;
                break;
            }
            case 0xD1:
            {
                n = source[i]; i++;
                if (n == 0x91) { n = 0xB7; break; }
                if (n >= 0x80 && n <= 0x8F) n = n + 0x6F;
                break;
            }
            }
        }
        m[0] = n; target = target + 1;
        if (target == letter_n)
            break;
    }
    return i;// target;
}

static void clearMSG()
{
    service.setMailOn(false);               // Отключить отсчет времени удаления сообщения через 10 минут
    service.setAllow_flashing(false);       // Запретить мигание сообщения

    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container[i].signal_source == 2)
        {
            Container_msg[i] = EmptyFO;
            Container[i] = EmptyFO;
        }
    }

    rows_Message.fillSprite(TFT_BLACK);
    rows_Message.pushToSprite(&back, 0, 25, TFT_BLACK);
    rows_Message.deleteSprite();
    back.pushSprite(0, 25);

    int screenWidth = tft.width();
    int screenHeight = tft.height();

    tft.setFreeFont(&FreeSerif12pt7b);
    tft.setTextColor(TFT_YELLOW);                   // Set character (glyph) color only (background not over-written)

    const char msg_txt[] = "MESSAGES DELETED";

    uint16_t curX = 40;                                      // Координаты вывода 
    uint16_t curY = 120;                                     // Координаты вывода текста
    tft.setCursor(curX, curY),                               // Set cursor for tft.print()

    tft.print(msg_txt);  // Отображаем 
    digitalWrite(SOC_GPIO_PIN_LED, LOW);
    vTaskDelay(400);
    digitalWrite(SOC_GPIO_PIN_LED, HIGH);
    vTaskDelay(3000);
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Вывод направления движения
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


float bearing_calc(float lat, float lon, float lat2, float lon2)
{

    float teta1 = radians(lat);
    float teta2 = radians(lat2);
    float delta1 = radians(lat2 - lat);
    float delta2 = radians(lon2 - lon);

    //==================Heading Formula Calculation================//

    float y = sin(delta2) * cos(teta2);
    float x = cos(teta1) * sin(teta2) - sin(teta1) * cos(teta2) * cos(delta2);
    float brng = atan2(y, x);
    brng = degrees(brng);// radians to degrees
    brng = (((int)brng + 360) % 360);
    return brng;

    /*
    *  // возвращает курс в градусах (Север=0, Запад=270) из позиции 1 в позицию 2,
 // оба указаны как широта и долгота в десятичных градусах со знаком.
 // Поскольку Земля не является точной сферой, расчетный курс может немного отклоняться.
 // С разрешения Маартена Ламерса

 double dlon = radians(long2-long1);
 lat1 = radians(lat1);
 lat2 = radians(lat2);
 double a1 = sin(dlon) * cos(lat2);
 double a2 = sin(lat1) * cos(lat2) * cos(dlon);
 a2 = cos(lat1) * sin(lat2) - a2;
 a2 = atan2(a1, a2);
 if (a2 < 0.0)
 {
   a2 += TWO_PI;
 }
 return degrees(a2);
    */
}

double distance_form(double lat1, double long1, double lat2, double long2)
{
    // возвращает расстояние в метрах между двумя указанными позициями
    // как десятичные градусы со знаком широты и долготы. Использует большой круг
    // расчет расстояния для гипотетической сферы радиусом 6372795 метров.
    // Поскольку Земля не является точной сферой, ошибки округления могут достигать 0,5%.
    // С разрешения Маартена Ламерса


    double delta = radians(long1 - long2);
    double sdlong = sin(delta);
    double cdlong = cos(delta);
    lat1 = radians(lat1);
    lat2 = radians(lat2);
    double slat1 = sin(lat1);
    double clat1 = cos(lat1);
    double slat2 = sin(lat2);
    double clat2 = cos(lat2);
    delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
    delta = sq(delta);
    delta += sq(clat2 * sdlong);
    delta = sqrt(delta);
    double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
    delta = atan2(delta, denom);
    return delta * 6372795;
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
#endif /* USE_TFT */



//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Вывод напряжения аккумулятора в процентах
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool array_countMax = false;
int sum_filtre = 0;
uint8_t array_count = 0;
uint8_t array_size = 50;
int dimension_array[50];


float battery_read()
{

    /*!! добавить гистерезис!!*/
    //read battery voltage per %
    long sum = 0;                  // sum of samples taken
    float voltage = 0.0;           // calculated voltage
    float output = 0.0;            // output value
    int val_voltage = 0;
    const float battery_max = 4.2; // maximum voltage of battery
    const float battery_min = 3.3; // minimum voltage of battery before shutdown

    float R1 = 330000.0;             // resistance of R1 (330K)
    float R2 = 430000.0;             // resistance of R2 (430K)
    int count_measurement = 40;      // количество измерений 
    float correction_factor = 3.3;   //?? поправочный коеффициент db

    for (int i = 0; i < count_measurement; i++)
    {
        sum += adc1_get_raw(ADC1_CHANNEL_2);
        delayMicroseconds(1000);
    }
    // calculate the voltage
    voltage = sum / (float)count_measurement;
    voltage = (voltage * 1.1) / 4096.0 * correction_factor;// 3.3; //for internal 1.1v reference
    voltage = voltage / (R2 / (R1 + R2));// *correction_factor;

    voltage = roundf(voltage * 100);

    /*Первичное заполнение фильтра при старте*/

    if (array_countMax == false)
    {
        for (int i = 0; i < array_size; i++)
        {
            dimension_array[i] = voltage;
        }

        array_count = array_size;          // 
        array_countMax = true;             //Разрешить выдавать данные об уровне напряжения аккумулятора
    }
    else
    {
        /*Массив заполнен первичными данными. Основной рачет напряжения*/
        array_count = array_size;          // 
        for (int i = 0; i < array_size; i++)
        {
            dimension_array[array_count] = dimension_array[array_count-1];
            array_count--;
            if (array_count == 0)
            {
               dimension_array[array_count] = voltage;
            }
        }
        array_count = array_size;
        for (int i = 0; i < array_size; i++)       //формируем первичные (заполняем массив) данные об уровне напряжения аккумулятора
        {
            sum_filtre += dimension_array[i]; // Вычисление суммы
        }
        val_voltage = sum_filtre / array_size;
    }

    sum_filtre = 0;                                         //
    voltage = (float)val_voltage/100;
    output = ((voltage - battery_min) / (battery_max - battery_min)) * 100;

    if (output < 100)
    {
        if (output < 0)
            output = 0;

        return output;
    }
    else
        return 100.0f;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


 
static void ESP32_Display_loop()
{
    char buf[16];
    uint32_t disp_value;

    uint16_t tbw;
    uint16_t tbh;

#if defined(USE_TFT)

 
    if (!TFT_display_frontpage) 
    {
        tft.fillScreen(TFT_NAVY);
        SOS_Sprite_on_off = false;                     // Режим отображения спрайта включен или потушен
        SOS_View_on_off   = false;                     // Флаг отображения включен или потушен
        new_SOS_flag      = false;                     // Флаг наличия импульсов SOS
        back.fillSprite(backColor);                    // Закрасим поле 
        backsprite.fillSprite(backColor);              // 
        angle = 0;// (360 - (int)ThisAircraft.course) % 360;

        /*Выполняем поворот по азимуту*/
        backsprite.pushRotated(&back, angle, TFT_BLACK);
        /***************    TFT_шкала дистанции    *******************/
        text_call = false;                            // Готов к выводу нового сообщения 
        back.pushSprite(0, 0);
        TFT_display_frontpage = true;
    }
    else 
    { /* TFT_display_frontpage  Основная программа отображения воздушной обстановки*/ 

        //-------------------- Блок работы с кнопкой  -----------------------------------------
        //******************** выполнение действий кнопок ******************************
        uint8_t new_buttton = service.get_num_buttton();

   /*     if (new_buttton == 1)
        {
            count_buttton_tmp++;
        }*/
        //if (new_buttton != 0 && new_buttton > 1)
        //{
        //    count_buttton_tmp = 350; 
        //}


        if (set_view_range != 0)
        {
            static uint32_t button_tmr = millis();
            if (millis() - button_tmr > BUTTON_OFF_DELAY)
            {
                button_tmr = millis();
                set_view_range = 0;
            }
        }

        if (new_buttton != 0/* && count_buttton_tmp == 350*/)
        {
          //  count_buttton_tmp = 0;
            switch (new_buttton) 
            {
            case 1:
                setMessageRead = service.getMessageRead();
                if (setMessageRead)
                {
                    service.setMessageRead(false);
                    esp_task_wdt_reset();
                    /* проконтролируем в КОМ порту количество неподтвержденных сообщений*/

                    //----------------------------------------------------------------------------------
                    bool confirm_message = service.get_confirm_message();
                    if (confirm_message)                                                              // Получен специальный код признака, означает что подтверждение не отправлено. Нужно отправить
                    {
                        confirm_message = false;                                                      // Запретить отправку подтверждения о прочтении сообщения (подтверждение отправлено).
                        char msgOK_Trecker[10] = "|OK";                                               // Формирование строки для ответного сообщения 
                        char msgNum[2] = "";                                                          // массив для записи номера ответного сообщения
                        //char msg[60] = "";                                                            // Массив для приема текстовых сообщений
                        char msg_resp[60] = "";                                                       // 
                        char msg_resp_tmp[60] = "";                                                   //

                        strcat(msgOK_Trecker, msgNum);                                                // Добавили в "|OK" номер ответного сообщения
                        strcat(msg_resp, msgOK_Trecker);                                              // Добавили к текущему ответу новый ответ. Формируем строку с несколькими ответами
                        SERIAL_TRACKER.println(msg_resp);                                             // Передать подтерждение о прочтении сообщения в буфер треккера

                        rows_Message.fillSprite(TFT_BLACK);                                           // Удаляем сообщение с экрана
                        rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);                            // Удаляем сообщение с экрана
                        rows_Message.deleteSprite();                                                  // Удаляем сообщение с экрана
                        service.setMailOn(false);                                                     // Отключить отсчет времени удаления сообщения через 10 минут
                        service.setAllow_flashing(false);                                             // Запретить мигание сообщения

                        back.pushSprite(0, 0);
                        strncpy(msg_mem_tmp, "", strlen(msg_mem_tmp));
                        strncpy(CommandHandler.msg_tmp_all, "", strlen(CommandHandler.msg_tmp_all));
                    }
                }
              
                break;
            case 2:
                //выполняется когда  var равно 2
                set_view_range++;
                if (set_view_range > 6)
                    set_view_range = 0;
                break;
            case 3:
                //выполняется когда  var равно 3
                clearMSG(); // Удалить все  сообщения
                break;
            default:
                // выполняется, если не выбрана ни одна альтернатива
                // default необязателен
                break;
            }

            service.set_num_buttton(0);
        }

        execution_state_SOS_button();

        //============================== Основной блок вывода воздушной обстановки на экран ========================================================
        static uint32_t tmr = millis();

        /* Проверяем наличие новой информации */
        if (millis() - tmr > DATA_MEASURE_THRESHOLD)
        {
            tmr = millis();
            int Air_txt_x = 41;              // Расположение текста в формуляре стороннего самолета 

            /* Проверяем есть ли данные GPS. */
            static uint32_t tmr1_GNSS = millis();
            if ((uint8_t)isValidGNSSFix() == true)
            {
                tmr1_GNSS = millis();
                text_call = false;
                fix_tmp = true;

            }

            if (millis() - tmr1_GNSS > DATA_VIEW_GPS_OFF)
            { 
                tmr1_GNSS = millis();
                fix_tmp = false;
              //  text_call = true;                            // Готов к выводу нового сообщения 
            }

            esp_task_wdt_reset();

          
            back.fillSprite(backColor);                   // Закрасим поле 
            backsprite.fillSprite(TFT_BLACK);             // 
            backsprite.setPivot(160, 160);                // Назначаем центр вращения спрайта воздушной обстановки


            if (!fix_tmp && (settings->mode != FLYRF_MODE_TXRX_TEST1) && (settings->mode != FLYRF_MODE_TXRX_TEST2) && (settings->mode != FLYRF_MODE_TXRX_TEST3) && (settings->mode != FLYRF_MODE_TXRX_TEST4) && (settings->mode != FLYRF_MODE_TXRX_TEST5)) // Эта проверка не проводится в тестовом режиме
            {
                text_call = true;          // 
                waiting_txt();                // Вывод сообщения о том что нет данных GPS
            }
            else
            {
                if (text_call)
                {
                    text_call = false;                             // Готов к выводу нового сообщения 
                    location_Message.deleteSprite();               // Удаляем сообщение "Нет сигнала GPS"с экрана
                }
            }

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
            thisAircraft_speed_tmr = val_speed;                  // Данные по скорости нашего самолета после фильтра


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

            if (view_alien_count >= 1)
            {
                esp_task_wdt_reset();

                /* Определяем какие пакеты приняты в текущем периоде*/
                /* Определяем минимальную дистанцию между нашим и сторонни самолетом и курс стороннего самолета*/
                //unsigned int min_distance = 32767;    // Запишем максимальное число для сравнения. Первоначально будем сравнивать
                unsigned int min_distance = 65534;      // Запишем максимальное число для сравнения. Первоначально будем сравнивать

                for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
                {
                    if (Container[i].addr && (now() - Container[i].timestamp) <= TFT_EXPIRATION_TIME)  // Если есть самолет в базе и подошло время обновления данных
                    {
                        // Serial.println(Container[i].addr, HEX);
                        isTeam_all[i] = true;     // Сторонние самолеты определены и зарегтстрированы в базе

                    /* вычисляем минимальное значение дистанции для переключения диапазона просмотра */
                        if (Container[i].latitude != 0 && Container[i].longitude != 0) // Расчет возможен если получены координаты стороннего самолета)
                        {
                            if ((int)Container[i].distance < min_distance)//если есть элемент, меньше нашего - делаем его минимальным
                            {
                                min_distance = (int)Container[i].distance;    // Сравниваем дистанции для определения наименшего расстояния
                                index_nearest_aircraft = i;                   // Записываем индекс ближайшего самолета  в базе
                                alient_course0 = Container[i].course;         // Записываем курс в градусах ближайшего чужого самолета
                                alient_speed0 = Container[i].speed;           // Записываем скорость ближайшего чужого самолета alien_speed_tmr[i]
                            }
                        }
                    }
                    else
                    {
                        /* нет данных по сторонним самолетам за длительный период  */

                        isTeam_all[i] = false;     // Сторонние самолеты определены и зарегтстрированы в базе?
                    }

                    esp_task_wdt_reset();
                }
                /* isAirDel = true;*/
                    /*==================================================================*/

                    /* Автоматический выбор диапазона отображения на основании минимальной дистанции от стороннего самолета */
 
                if (set_view_range == 0)
                {
                    if (min_distance > 16000)
                    {
                        divider = 32000;  // 32000
                        divider_num = 1;
                    }
                    else if (min_distance <= 16000 && min_distance > 8000)  //16000/2
                    {
                        divider = 16000;  // 16000
                        divider_num = 2;
                    }
                    else if (min_distance <= 8000 && min_distance > 4000) // 8000 /2
                    {
                        divider = 8000;  // 8000
                        divider_num = 3;
                    }
                    else if (min_distance <= 4000 && min_distance > 2000) // 4000/2
                    {
                        divider = 4000; // 4000
                        divider_num = 4;
                    }
                    else if (min_distance <= 2000 && min_distance > 1000) // 2000/2
                    {
                        divider = 2000;  //2000
                        divider_num = 5;
                    }
                    else if (min_distance <= 1000 && min_distance > 500) //1000 /2
                    {
                        divider = 1000;  //1000m
                        divider_num = 6;
                    }
                    else if (min_distance <= 500 && min_distance > 200) // 500/2
                    {
                        divider = 500;  // 500 m
                        divider_num = 7;
                    }
                    else if (min_distance <= 200 && min_distance > 100) //200/2
                    {
                        divider = 200;  // 200 m
                        divider_num = 8;
                    }
                    else if (min_distance <= 100)   // 100/2
                    {
                        divider = 100; // 100 m
                        divider_num = 9;
                    }
                }



                // Установки определения уровней предупреждения. Параметры задаются со смартфона и записываются в EEPROM

                // settings->alarm_attention;     // Внимание. Параметр - расстояние 
                // settings->alarm_warning;       // Предупреждение. Параметр - расстояние 
                // settings->alarm_danger;        // Тревога. Параметр - расстояние        
                // settings->alarm_height;        // Тревога по высоте. Параметр - высота 


                //===================================================================================

                bool rssi_off = false;

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

                            /*Функция проверяет и если надо задает новое значение, так чтобы оно была в области допустимых значений, заданной параметрами.*/
                            new_rel_x = constrain(((int)Container[i].distance / 2) * sin(radians(new_angle[i])), -32768, 32767);
                            new_rel_y = constrain(((int)Container[i].distance / 2) * cos(radians(new_angle[i])), -32768, 32767);

                            new_x = ((int32_t)new_rel_x * (int32_t)radius) / divider;
                            new_y = ((int32_t)new_rel_y * (int32_t)radius) / divider;

                            Container_alien_X[i] = new_x;  // Сохранить координаты стороннего самолета
                            Container_alien_Y[i] = new_y;

                            /* Расчет координат формуляра стороннего самолета */
                            /* Определяем расположение формуляра на экране слева или справа*/

                            if (new_x >= 0)  // Зона правая сторона?
                            {
                                Air_txt_left[i] = false;
                            }
                            else   //Зона левая сторона?
                            {
                                Air_txt_left[i] = true;
                            }


                            if (height_difference[i] >= 0)  //height_difference[i] >= 0
                            {

                                form_x = new_x - 23;     // Спрайт текста  находтся ниже самолета
                                form_y = new_y + 22;     // 21
                            }
                            else
                            {
                                form_x = new_x - 23;    // Спрайт текста  находтся выше самолета
                                form_y = new_y - 9;     // -8
                            }

                            Container_logbook_X[i] = form_x;  // Сохранить координаты формуляра стороннего самолета
                            Container_logbook_Y[i] = form_y;

                            /* Расчет координат стрелок стороннего самолета */
                            /* Определяем расположение стрелок на экране слева или справа*/
                            if (new_x >= 0)  // Зона правая сторона?
                            {
                                if (alient_course[i] >= 0 && alient_course[i] <= 180)
                                {
                                    if (new_y <= -62) //  
                                    {
                                        form_arrow_x = new_x - 18; //16 Спрайт стрелки находтся xx от самолета
                                        form_arrow_y = new_y + 4; //
                                    }
                                    else
                                    {
                                        form_arrow_x = new_x - 18; //16 Спрайт стрелки находтся xx от самолета
                                        form_arrow_y = new_y + 4;     //
                                    }
                                }
                                else
                                {
                                    if (new_y <= -62) //
                                    {
                                        form_arrow_x = new_x + 9; //8 Спрайт стрелки  находтся xx самолета
                                        form_arrow_y = new_y + 4; //
                                    }
                                    else
                                    {
                                        form_arrow_x = new_x + 9; //8 Спрайт стрелки  находтся xx самолета
                                        form_arrow_y = new_y + 4;     //
                                    }
                                }
                            }
                            else   //Зона левая сторона?
                            {
                                if (alient_course[i] >= 0 && alient_course[i] <= 180)
                                {
                                    if (new_y <= -62) // 
                                    {
                                        form_arrow_x = new_x - 18; //16 Спрайт стрелки  находтся слева от самолета
                                        form_arrow_y = new_y + 4; //
                                    }
                                    else
                                    {
                                        form_arrow_x = new_x - 18; // 16Спрайт стрелки  находтся слева от самолета
                                        form_arrow_y = new_y + 4; //1
                                    }
                                }
                                else
                                {
                                    if (new_y <= -62) //
                                    {
                                        form_arrow_x = new_x + 9; //8 Спрайт стрелки находтся справа самолета
                                        form_arrow_y = new_y + 4; //
                                    }
                                    else
                                    {
                                        form_arrow_x = new_x + 9; //8 Спрайт стрелки  находтся справа самолета
                                        form_arrow_y = new_y + 4; //1
                                    }
                                }
                            }

                            Container_arrow_X[i] = form_arrow_x;  // Сохранить координаты формуляра стороннего самолета
                            Container_arrow_Y[i] = form_arrow_y;

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
                            Air_txt_Sprite[i]->fillSprite(TFT_BLACK);                        // Закрасим поле соообщений 
                            //Air_txt_Sprite[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 14, TFT_RED); //!! Только для теста
                            Air_txt_Sprite[i]->setTextColor(little_air_color[i], backColor); // Установить цвет согласно программе предупреждения опастности
                            Air_txt_Sprite[i]->setTextDatum(TC_DATUM);                       // Определим как будет выводится текст
                            Air_txt_Sprite[i]->loadFont(NotoSansBold15);                     // Установить шрифт формуляра


                            /*===============  Этот фрагмент для вывода самолета в движении*/
                            /* Запись параметров высоты в формуляр */

                            if (Container[i].addr)
                            {
                                int height_tmp = (int)round(height_difference[i] / 10);

                                if (height_tmp > 0)
                                {
                                    Air_txt_Sprite[i]->drawString("+" + String(height_tmp), 27, 1, 0);
                                }
                                else
                                {
                                    Air_txt_Sprite[i]->drawString(String(height_tmp), 27, 1, 0);
                                }

                            }
                            /* Записать скорость в формуляр  движущегося самолета*/
                            alien_speed_view[i] = 50 - ((int)Container[i].speed / 60 * 3);  // Расстояние стороннего самолета для вывода на дисплей

                            if (alien_speed_view[i] > 40)
                            {
                                alien_speed_view[i] = 40;
                            }

                            /*
                                Действительно для самолетов с известными координатами.
                                Определяем наличие и направление вывода стрелок.
                                Источник данных не важен
                            */

                            switch (arrow_up_down[i])
                            {
                            case 0:
                                arrow[i]->fillSprite(TFT_BLACK);                      // Закрасим поле стрелок вверх
                                break;
                            case 1:
                                /*Рисуем стрелку вверх */
                                arrow[i]->fillSprite(TFT_BLACK);                       // Закрасим поле стрелок вверх
                                arrow[i]->drawLine(4, 0, 4, 9, little_air_color[i]);   // |
                                arrow[i]->drawLine(0, 4, 3, 1, little_air_color[i]);   // /
                                arrow[i]->drawLine(5, 1, 8, 4, little_air_color[i]);   // 
                                break;
                            case 2:
                                /*Рисуем стрелку вниз */
                                arrow[i]->fillSprite(TFT_BLACK);                       // Закрасим поле стрелок вниз
                                arrow[i]->drawLine(4, 0, 4, 9, little_air_color[i]);   // |
                                arrow[i]->drawLine(0, 5, 3, 8, little_air_color[i]);   // 
                                arrow[i]->drawLine(5, 8, 8, 5, little_air_color[i]);   //
                                break;
                            default:
                                break;
                            }

                            // Формируем изображение летящего объекта с учетом с какого источника были полученыданные о координатах
                            if (Container[i].signal_source == 0) // С учетом данных, полученных с приемника LoRa868
                            {
                                /*Рисуем маленький самолетик в виде закрашенного круга */
                                airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле самолетика
                                airplane[i]->fillCircle(50, 50, 5, little_air_color[i]),                     // fillCircle //drawCircle
                                    airplane[i]->drawLine(50, 45, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                                area_airplane[i]->fillSprite(TFT_BLACK);                                          // Закрасим поле 

                                // area_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_YELLOW); //!! Только для теста
                                // LoRa_airplane[i]->drawSmoothRoundRect(0, 0, 1, 1, 99, 99, TFT_PINK); //!! Только для теста
                            }
                            else if (Container[i].signal_source == 1 || Container[i].signal_source == 2)  // С учетом данных, полученных с приемника DUMP1090
                            {
                                /*Рисуем маленький самолетик */
                                airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле самолетика

                                airplane[i]->drawLine(49, 43, 49, 57, little_air_color[i]);
                                airplane[i]->drawLine(50, 42, 50, 57, little_air_color[i]);
                                airplane[i]->drawLine(51, 43, 51, 57, little_air_color[i]);


                                airplane[i]->drawLine(48, 46, 52, 46, little_air_color[i]);
                                airplane[i]->drawLine(47, 47, 53, 47, little_air_color[i]);
                                airplane[i]->drawLine(46, 48, 54, 48, little_air_color[i]);
                                airplane[i]->drawLine(45, 49, 55, 49, little_air_color[i]);
                                airplane[i]->drawLine(44, 50, 47, 50, little_air_color[i]);
                                airplane[i]->drawLine(53, 50, 56, 50, little_air_color[i]);
                                airplane[i]->drawLine(43, 51, 45, 51, little_air_color[i]);
                                airplane[i]->drawLine(55, 51, 57, 51, little_air_color[i]);
                                airplane[i]->drawLine(42, 52, 43, 52, little_air_color[i]);
                                airplane[i]->drawLine(57, 52, 58, 52, little_air_color[i]);

                                airplane[i]->drawLine(48, 56, 52, 56, little_air_color[i]);
                                airplane[i]->drawLine(47, 57, 53, 57, little_air_color[i]);
                                airplane[i]->drawLine(46, 58, 48, 58, little_air_color[i]);
                                airplane[i]->drawLine(52, 58, 54, 58, little_air_color[i]);
                                airplane[i]->drawLine(45, 59, 46, 59, little_air_color[i]);
                                airplane[i]->drawLine(54, 59, 55, 59, little_air_color[i]);

                                airplane[i]->drawLine(50, 41, 50, alien_speed_view[i] - 4, little_air_color[i]); // Рисуем прямую линию"скорости" с носа самолета
                                area_airplane[i]->fillSprite(TFT_BLACK);      // Закрасим поле 
                            }


                            if (isTeam_all[i] == true)
                            {

                                airplane[i]->pushRotated(area_airplane[i], alient_course[i], TFT_BLACK); // 
                                area_airplane[i]->pushToSprite(&back, radar_center_x + Container_alien_X[i] - 50, radar_center_y - Container_alien_Y[i] - 50, TFT_BLACK);
                                arrow[i]->pushToSprite(&back, radar_center_x + Container_arrow_X[i], radar_center_y - Container_arrow_Y[i], TFT_BLACK);
                                Air_txt_Sprite[i]->pushToSprite(&back, radar_center_x + Container_logbook_X[i], radar_center_y - Container_logbook_Y[i], TFT_BLACK);

                                isTeam_all[i] = false;
                                esp_task_wdt_reset();
                            }



                            esp_task_wdt_reset();

                        } //Закочить обработку данных самолетов с известными координатами

                        //============================= Конец обработки данных самолетов самолетов  с известными координатами ==========================
                    }

                    if (settings->rssi_view == VIEW_RSSI_ON)
                    {
                        back.setTextDatum(0);
                        back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                        int rssi_y = 178;

                        if (settings->ram_view == VIEW_RAM_ON)
                        {
                            rssi_y = 178;
                        }
                        else
                        {
                            rssi_y = 194;
                        }

                        if (Container[i].rssi < 0 && Container[i].signal_source == 0)
                        {
                            back.drawString("      ", 1, 210);
                            back.drawString(String(Container[i].rssi) + " db", 20, rssi_y);
                            rssi_off = true;
                        }
                   
                        if (!rssi_off)
                        {
                            back.drawString("      ", 20, rssi_y);
                        }
                    }
                }
            }
            else
            {
            int rssi_y = 178;

            if (settings->ram_view == VIEW_RAM_ON)
            {
                rssi_y = 178;
            }
            else
            {
                rssi_y = 194;
            }
                back.drawString("      ", 1, rssi_y);
                divider = 32000;  // 
                divider_num = 1;
            }


            //============================== Формируем неподвижное базовое изображение на экране =========================================== 

                /* настройки вывода времени на экран*/
            time_info.setFreeFont(&FreeSerif12pt7b);
            time_info.fillSprite(TFT_BLACK);
            time_info.setTextDatum(CC_DATUM);
            time_info.setTextColor(TFT_GREEN, backColor);

            if (gnss.time.isValid()) //Проверяем есть ли данные GPS.
            {
                String hour_tmp;

                if (gnss.time.hour() < 10)
                {
                    hour_tmp = "0";
                    hour_tmp += String(gnss.time.hour());
                }
                else
                {
                    hour_tmp = String(gnss.time.hour());
                }
                String min_tmp;
                if (gnss.time.minute() < 10)
                {
                    min_tmp = "0";
                    min_tmp += String(gnss.time.minute());
                }
                else
                {
                    min_tmp = String(gnss.time.minute());
                }
                time_info.drawString(hour_tmp + ":" + min_tmp, 29, 11);
            }
            else
            {
                time_info.drawString("--:--", 29, 11);
            }

            time_info.pushToSprite(&back, 5, 1);        // Отображаем табло времени

            Draw_circular_scale();

            ///* Вычисляем направление полета нашего самолета*/
            if (ThisAircraft.latitude != Aircraft_latitude_old1)
            {
                test_curse = bearing_calc(Aircraft_latitude_old1, Aircraft_longitude_old1, ThisAircraft.latitude, ThisAircraft.longitude);

                Aircraft_latitude_old1 = ThisAircraft.latitude;
                Aircraft_longitude_old1 = ThisAircraft.longitude;
            }

            esp_task_wdt_reset();

            /*Выполняем поворот нашего самолета по азимуту*/
            backsprite.pushRotated(&back, angle, TFT_BLACK);

            /* Определяем масштаб в ручном режиме */

            if (set_view_range != 0)
            {
                switch (set_view_range)
                {
                case 1:
                    divider = 32000;  // 32000
                    divider_num = 1;
                    break;
                case 2:
                    divider = 16000;  // 16000
                    divider_num = 2;
                    break;
                case 3:
                    divider = 8000;  // 8000
                    divider_num = 3;
                    break;
                case 4:
                    divider = 4000; // 4000
                    divider_num = 4;
                    break;
                case 5:
                    divider = 2000;  //2000
                    divider_num = 5;
                    break;
                case 6:
                    divider = 1000;  //1000m
                    divider_num = 6;
                    break;
                default:
                    divider = 32000;  // 32000
                    divider_num = 1;
                    break;
                }

            }
            esp_task_wdt_reset();
            if (divider <= 32767)
            {
                back.loadFont(NotoSansBold15);
                back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                back.setTextDatum(TC_DATUM);
                int data_KM_x = 166;  // Расположение строки по X
                int data_KM_y = 223;  // Расположение строки по Y

                switch (divider_num)
                {
                case 1:
                    back.drawString("32 km", data_KM_x - 3, data_KM_y);
                    break;
                case 2:
                    back.drawString("16 km", data_KM_x - 3, data_KM_y);
                    break;
                case 3:
                    back.drawString("8 km", data_KM_x - 2, data_KM_y);
                    break;
                case 4:
                    back.drawString("4 km", data_KM_x - 2, data_KM_y);
                    break;
                case 5:
                    back.drawString("2 km", data_KM_x - 2, data_KM_y);
                    break;
                case 6:
                    back.drawString("1 km", data_KM_x - 2, data_KM_y);
                    break;
                case 7:
                    back.drawString("500 m", data_KM_x - 1, data_KM_y);
                    break;
                case 8:
                    back.drawString("200 m", data_KM_x - 1, data_KM_y);
                    break;
                case 9:
                    back.drawString("100 m", data_KM_x, data_KM_y);
                    break;
                default:
                    back.drawString("32 km", data_KM_x - 3, data_KM_y);
                    break;
                    // выполняется, если не выбрана ни одна альтернатива
                }

                if (set_view_range != 0)
                {

                    back.drawRect(data_KM_x - 30, data_KM_y - 1, 56, 17, TFT_RED);

                }
            }

            if (settings->ram_view == VIEW_RAM_ON)
            {
                back.setTextDatum(0);
                back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                back.drawString("Free: " + String(ESP.getFreeHeap()), 1, 194);
            }

            if ((settings->view_test_coord == VIEW_TEST_COORD_ON) && (settings->mode == FLYRF_MODE_TXRX_TEST1) || (settings->mode == FLYRF_MODE_TXRX_TEST2) || (settings->mode == FLYRF_MODE_TXRX_TEST3) || (settings->mode == FLYRF_MODE_TXRX_TEST4) || (settings->mode == FLYRF_MODE_TXRX_TEST5)) // Эта проверка проводится в тестовом режиме
            {
                back.setTextDatum(0);
                back.setTextColor(TFT_DARKGREY, TFT_BLACK);
                back.drawString("Lat: " + String(ThisAircraft.test_latitude,5), 1, 210);
                back.drawString("Lon: " + String(ThisAircraft.test_longitude,5), 1, 226);
            }
               
            if (settings->akk_view == VIEW_AKK_ON)
            {
                float akk = battery_read();
                int val_x = map(akk, 0, 100, 0, 26);

                char str_Vcc[4];
                String str_pr = "%";
                dtostrf(akk, 3, 0, str_Vcc);

                power1.setTextDatum(CL_DATUM);
                power1.setFreeFont(&FreeSerif9pt7b);

                /*Рисуем заряд аккумулятора*/
                if (akk > 40.0)
                {
                    power1.fillSprite(TFT_BLACK);
                    power1.fillRect(2, 2, 26, 12, TFT_BLACK);
                    power1.fillRect(2, 2, val_x, 12, TFT_GREEN);
                    power1.drawRect(0, 0, 30, 16, TFT_WHITE);
                    power1.fillRect(30, 4, 3, 8, TFT_WHITE);
                    power1.setTextColor(TFT_GREEN, TFT_BLACK);
                    power1.drawString(str_Vcc + str_pr, 33, 7);
                    power1.pushToSprite(&back, 244, 4, TFT_BLACK);
                }
                else if (akk <= 40.0 && akk >= 15.1)
                {
                    power1.fillSprite(TFT_BLACK);
                    power1.fillRect(2, 2, 26, 12, TFT_BLACK);
                    power1.fillRect(2, 2, val_x, 12, TFT_ORANGE);
                    power1.drawRect(0, 0, 30, 16, TFT_ORANGE);
                    power1.fillRect(30, 4, 3, 8, TFT_ORANGE);
                    power1.setTextColor(TFT_ORANGE, TFT_BLACK);
                    power1.drawString(str_Vcc + str_pr, 33, 7);
                    power1.pushToSprite(&back, 244, 4, TFT_BLACK);
                }
                else
                {
                    power1.fillSprite(TFT_BLACK);
                    power1.fillRect(2, 2, 26, 12, TFT_BLACK);
                    power1.fillRect(2, 2, val_x, 12, TFT_RED);
                    power1.drawRect(0, 0, 30, 16, TFT_RED);
                    power1.fillRect(30, 4, 3, 8, TFT_RED);
                    power1.setTextColor(TFT_RED, TFT_BLACK);
                    power1.drawString(str_Vcc + str_pr, 33, 7);
                    power1.pushToSprite(&back, 244, 4, TFT_BLACK);
                }
            }


            esp_task_wdt_reset();


            /*Формируем картинку нашего самолета*/
                /* Рисуем фюзеляж*/
            int width_air = 148;
            int height_air = 150;
            back.drawLine(12 + width_air, 0 + height_air, 12 + width_air, 18 + height_air, TFT_DARKGREY);

            /*Рисуем передние крылья*/
            back.drawLine(3 + width_air, 7 + height_air, 20 + width_air, 7 + height_air, TFT_DARKGREY);
            back.drawLine(0 + width_air, 8 + height_air, 23 + width_air, 8 + height_air, TFT_DARKGREY);

            /*Рисуем задние крылья*/
            back.drawLine(7 + width_air, 17 + height_air, 17 + width_air, 17 + height_air, TFT_DARKGREY);
            //============================== Конец формирования неподвижного базовоо изображения на экране =========================================== 



            bool DUMP1090_tmp = moduleDump1090.getNewDUMP_0_Flag();
            if (DUMP1090_tmp)
            {

                DUMP1090_info.drawLine(12, 6, 12, 19, TFT_WHITE);
                DUMP1090_info.drawLine(13, 5, 13, 19, TFT_WHITE);
                DUMP1090_info.drawLine(14, 6, 14, 19, TFT_WHITE);

                DUMP1090_info.drawLine(9, 9, 17, 9, TFT_WHITE);
                DUMP1090_info.drawLine(7, 10, 19, 10, TFT_WHITE);
                DUMP1090_info.drawLine(5, 11, 21, 11, TFT_WHITE);

                DUMP1090_info.drawLine(9, 18, 17, 18, TFT_WHITE);
                DUMP1090_info.drawLine(8, 19, 18, 19, TFT_WHITE);

                DUMP1090_info.drawCircle(13, 13, 11, TFT_DARKGREY);

                DUMP1090_info.pushToSprite(&back, 228, 0, TFT_BLACK);


                static uint32_t tmr_DUMP1090 = millis();
                if (millis() - tmr_DUMP1090 > DUMP_FLASHING_OFF)
                {
                    tmr_DUMP1090 = millis();
                    moduleDump1090.setNewDUMP_0_Flag(false);
                }

                esp_task_wdt_reset();
            }

            //============================== Блок работы с текстовыми сообщениями ===============================================================

                /* Проверить пришло ли новое сообщение. */
            bool new_flag = service.getNewMessageFlag();                                 // Получить признак нового сообщения 

            if (new_flag)                                                                // если новое сообщение
            {
                service.setMailOn(true);                                                 // Начинаем отсчет времени удаления сообщения через 10 минут
                service.setNewMessageFlag(false);                                        // Сбросить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
                service.set_confirm_message(true);                                       // Разрешить отправить подтверждение прочтения сообщения.
                service.setAllow_flashing(true);                                         // Разрешить мигание сообщения

                strncpy(msg_mem_tmp, "", strlen(msg_mem_tmp)+2);                           // Очистить временный буфер
                strncpy(msg_mem_tmp, CommandHandler.msg_tmp_all, strlen(CommandHandler.msg_tmp_all)); // Записать новое сообщение во временный буфер
                strncpy(CommandHandler.msg_tmp_all, "", strlen(CommandHandler.msg_tmp_all)+2); // Очистить буфер сообщения
                drawMessage();                                                          // вызвать программу отображения информации на дисплее
            }

            previousMillis_msg = flashing_on_off ? DATA_FLASHING_ON : DATA_FLASHING_OFF;

            bool Allow_flashing1 = service.getAllow_flashing();
            if (Allow_flashing1)
            {
                static uint32_t tmr_flashing = millis();
                if (millis() - tmr_flashing >= previousMillis_msg)
                {
                    tmr_flashing = millis();
                    if (flashing_on_off == true)
                    {
                        flashing_on_off = false;
                        rows_Message.fillSprite(TFT_BLACK);
                        rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
                        rows_Message.deleteSprite();
                    }
                    else
                    {
                        flashing_on_off = true;
                        drawMessage();     // вызвать программу отображения информации на дисплее
                    }
                }

                if (flashing_on_off)
                {
                    drawMessage();     // вызвать программу отображения информации на дисплее
                }
            }

            if (SOS_View_on_off)
            {
                display_state_SOS_button();                                                 // вызвать программу отображения состояния кнопки SOS
                state_SOS_button.drawString("SOS", 43, 22);
                state_SOS_button.pushToSprite(&back, 117, 20, TFT_BLACK);
            }
            else
            {
                if (state_SOS_button.created())
                {
                  state_SOS_button.deleteSprite();
                }
            }

            if (CommandHandler.clear_message)
            {
                CommandHandler.clear_message = false;
                clearMSG(); // Удалить всю почту
            }

            /* Запустить программу отсчета времени удаления сообщения через 10 минут*/

            bool flags_MailOn = service.getMailOn();

            if (flags_MailOn)
            {
                static uint32_t mail_off = millis();
                if (millis() - mail_off > MAIL_OFF_DELAY)                                  // Убрать отображение сообщения почты
                {
                    mail_off = millis();
                    service.setMailOn(true);                                                // Время 10 минут закончилось
                    service.setAllow_flashing(false);                                       // Запретить мигание сообщения
                    rows_Message.fillSprite(TFT_BLACK);
                    rows_Message.pushToSprite(&back, 0, 0, TFT_BLACK);
                    rows_Message.deleteSprite();
                }
            }
            esp_task_wdt_reset();

            /*рисуем все спрайты*/

            // vTaskDelay(100);
            back.pushSprite(0, 0); //!! Временно
            back.pushSprite(0, 0);
        }
    }
 

#endif /* USE_TFT */

#if defined(USE_OLED)
        OLED_loop();
#endif /* USE_OLED */

}




static void ESP32_Display_fini(int reason)
{

}


static bool ESP32_Baro_setup()
{

     /* Start from 1st I2C bus */

    Wire.setPins(SOC_GPIO_PIN_SDA, SOC_GPIO_PIN_SCL);
    if (Baro_probe())
      return true;

    WIRE_FINI(Wire);

  return true;
}


static void ESP32_WDT_setup()
{
  enableLoopWDT();
}

static void ESP32_WDT_fini()
{
  disableLoopWDT();
}


#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)

#define USB_TX_FIFO_SIZE (MAX_TRACKING_OBJECTS * 65 + 75 + 75 + 42 + 20)
#define USB_RX_FIFO_SIZE (256)

#if defined(USE_USB_HOST)

#include <cp210x_usb.hpp>
#include <ftdi_usb.hpp>
#include <ch34x_usb.hpp>

using namespace esp_usb;

#define USB_MAX_WRITE_CHUNK_SIZE    64

#define USB_HOST_PRIORITY           20

#undef  TAG
#define TAG "USB-CDC"

cbuf *USB_RX_FIFO, *USB_TX_FIFO;

// CDC-ACM driver object
typedef struct {
    usb_host_client_handle_t cdc_acm_client_hdl;        /*!< USB Host handle reused for all CDC-ACM devices in the system */
    SemaphoreHandle_t open_close_mutex;
    EventGroupHandle_t event_group;
    cdc_acm_new_dev_callback_t new_dev_cb;
    SLIST_HEAD(list_dev, cdc_dev_s) cdc_devices_list;   /*!< List of open pseudo devices */
} cdc_acm_obj_t;

extern cdc_acm_obj_t *p_cdc_acm_obj;

ESP32_USBSerial_device_t ESP32_USB_Serial = {
    .connected = false,
    .index = 0,
};

CdcAcmDevice *cdc = new CdcAcmDevice();

enum {
    USBSER_TYPE_CDC,
    USBSER_TYPE_CP210X,
    USBSER_TYPE_FTDI,
    USBSER_TYPE_CH34X,
};

const USB_Device_List_t supported_USB_devices[] = {
  { 0x0483, 0x5740, USBSER_TYPE_CDC, FLYRF_MODEL_DONGLE, "Dongle" /* or Bracelet */, "Edition" },
  { 0x239A, 0x8029, USBSER_TYPE_CDC, FLYRF_MODEL_BADGE, "Badge", "Edition" },
  { 0x2341, 0x804d, USBSER_TYPE_CDC, FLYRF_MODEL_ACADEMY, "Academy", "Edition" },
  { 0x2886, 0x802f, USBSER_TYPE_CDC, FLYRF_MODEL_ACADEMY, "Academy", "Edition" },
  { 0x1d50, 0x6089, USBSER_TYPE_CDC, FLYRF_MODEL_ES, "ES", "Edition" },
  { 0x2e8a, 0x000a, USBSER_TYPE_CDC, FLYRF_MODEL_LEGO, "Lego", "Edition" },
  { 0x2e8a, 0xf00a, USBSER_TYPE_CDC, FLYRF_MODEL_LEGO, "Lego", "Edition" },
  { 0x1A86, 0x55D4, USBSER_TYPE_CDC, FLYRF_MODEL_PRIME_MK2, "CH9102", "device" },
  { 0x303a, 0x8133, USBSER_TYPE_CDC, FLYRF_MODEL_PRIME_MK3, "Prime 3", "Edition" },
  { 0x15ba, 0x0044, USBSER_TYPE_CDC, FLYRF_MODEL_BALKAN, "Balkan", "Edition" },
  { 0x303a, 0x8132, USBSER_TYPE_CDC, FLYRF_MODEL_STANDALONE, "Standalone", "Edition" },
  { 0x10c4, 0xea60, USBSER_TYPE_CP210X, FLYRF_MODEL_UNKNOWN, "CP210X", "device" },
  { 0x0403, 0x6001, USBSER_TYPE_FTDI, FLYRF_MODEL_UNKNOWN, "FT232", "device" },
  { 0x1a86, 0x7523, USBSER_TYPE_CH34X, FLYRF_MODEL_UNKNOWN, "CH340", "device" },
};

enum {
  FLYRF_DEVICE_COUNT =
      sizeof(supported_USB_devices) / sizeof(supported_USB_devices[0])
};

static void handle_rx(uint8_t *data, size_t data_len, void *arg)
{
//    ESP_LOGI(TAG, "Data received");
//    ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_INFO);
      if (data_len > 0) {
        USB_RX_FIFO->write((char *) data,
                     USB_RX_FIFO->room() > data_len ?
                     data_len : USB_RX_FIFO->room());
      }
}

void usb_lib_task(void *arg)
{
    while (1) {
        //Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "All clients deregistered");
            /*ESP_ERROR_CHECK*/(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            break;
        }
    }

    vTaskDelete(NULL);
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %d", event->data.error);
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGI(TAG, "Device suddenly disconnected");
//            xSemaphoreGive(device_disconnected_sem);
#if 0
            if (ESP32_USB_Serial.device) {
              ESP32_USB_Serial.device->close();
              ESP32_USB_Serial.device = NULL;
            }
            usb_host_device_free_all();
#endif
            ESP32_USB_Serial.connected = false;
            break;
        case CDC_ACM_HOST_SERIAL_STATE:
            ESP_LOGI(TAG, "serial state notif 0x%04X", event->data.serial_state.val);
            break;
        case CDC_ACM_HOST_NETWORK_CONNECTION:
        default: break;
    }
}

static void ESP32SX_USB_setup()
{
    USB_RX_FIFO = new cbuf(USB_RX_FIFO_SIZE);
    USB_TX_FIFO = new cbuf(USB_TX_FIFO_SIZE);

    //Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), USB_HOST_PRIORITY, NULL);

    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
}

static void ESP32SX_USB_loop()
{
    if (!ESP32_USB_Serial.connected) {
        ESP_LOGD(TAG, "Checking list of connected USB devices");
        uint8_t dev_addr_list[10];
        int num_of_devices;
        ESP_ERROR_CHECK(usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_of_devices));

        // Go through device address list and find the one we are looking for
        for (int i = 0; i < num_of_devices; i++) {
            usb_device_handle_t current_device;
            // Open USB device
            if (usb_host_device_open(p_cdc_acm_obj->cdc_acm_client_hdl, dev_addr_list[i], &current_device) != ESP_OK) {
                continue; // In case we failed to open this device, continue with next one in the list
            }
            assert(current_device);
            const usb_device_desc_t *device_desc;
            ESP_ERROR_CHECK(usb_host_get_device_descriptor(current_device, &device_desc));

            uint16_t vid = device_desc->idVendor;
            uint16_t pid = device_desc->idProduct;
            uint8_t dev_type;

            usb_host_device_close(p_cdc_acm_obj->cdc_acm_client_hdl, current_device);

            ESP_LOGI(TAG, "USB device detected, VID: %X, PID: %X", vid, pid);

            int j;
            for (j = 0; j < FLYRF_DEVICE_COUNT; j++) {
              if (vid == supported_USB_devices[j].vid &&
                  pid == supported_USB_devices[j].pid) {
                dev_type = supported_USB_devices[j].type;
                break;
              }
            }

            if (j < FLYRF_DEVICE_COUNT) {
              const cdc_acm_host_device_config_t dev_config = {
                  .connection_timeout_ms = 5000,
                  .out_buffer_size = 64,
                  .event_cb = handle_event /* NULL */,
                  .data_cb = handle_rx,
                  .user_arg = NULL,
              };

              CdcAcmDevice *vcp;

              cdc_acm_line_coding_t line_coding = {
                  .dwDTERate = SERIAL_OUT_BR,
                  .bCharFormat = 0,
                  .bParityType = 0,
                  .bDataBits = 8,
              };

              switch (dev_type)
              {
              case USBSER_TYPE_CDC:
                try {
                    ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X", vid, pid);
                    cdc->open(vid, pid, 0, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                vcp = cdc;
                break;

              case USBSER_TYPE_CP210X:
                try {
                    ESP_LOGI(TAG, "Opening CP210X device");
                    vcp = CP210x::open_cp210x(pid, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                break;

              case USBSER_TYPE_FTDI:
                try {
                    ESP_LOGI(TAG, "Opening FT232 device");
                    vcp = FT23x::open_ftdi(pid, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                break;

              case USBSER_TYPE_CH34X:
                try {
                    ESP_LOGI(TAG, "Opening CH340 device");
                    vcp = CH34x::open_ch34x(pid, &dev_config);
                }

                catch (esp_err_t err) {
                    ESP_LOGE(TAG, "The required device was not opened.\nExiting...");
                    continue;
                }
                break;
              }

              ESP_ERROR_CHECK(vcp->line_coding_set(&line_coding));
              ESP_LOGI(TAG, "Line Set: Rate: %d, Stop bits: %d, Parity: %d, Databits: %d", line_coding.dwDTERate,
                       line_coding.bCharFormat, line_coding.bParityType, line_coding.bDataBits);

              ESP_ERROR_CHECK(vcp->set_control_line_state(true, true));

              ESP32_USB_Serial.connected = true;
              ESP32_USB_Serial.device = vcp;
              ESP32_USB_Serial.index = j;

            } else {
              ESP_LOGI(TAG, "USB device VID: %X, PID: %X is not supported", vid, pid);
            }
        }
    } else {
#if 0
        uint8_t dev_addr_list[10];
        int num_of_devices;
        ESP_ERROR_CHECK(usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_of_devices));
        if (num_of_devices == 0) {
          ESP_LOGI(TAG, "Closing USB device 0x%04X:0x%04X",
                   supported_USB_devices[ESP32_USB_Serial.index].vid,
                   supported_USB_devices[ESP32_USB_Serial.index].pid);
          if (ESP32_USB_Serial.device) {
            ESP32_USB_Serial.device->close();
            ESP32_USB_Serial.device = NULL;
          }
          ESP32_USB_Serial.connected = false;
          USB_TX_FIFO->flush();
          USB_RX_FIFO->flush();
        }
        else
#endif
        {
          uint8_t chunk[USB_MAX_WRITE_CHUNK_SIZE];
          size_t size = (USB_TX_FIFO->available() < USB_MAX_WRITE_CHUNK_SIZE ?
                         USB_TX_FIFO->available() : USB_MAX_WRITE_CHUNK_SIZE);

          if (size > 0) {
            USB_TX_FIFO->read((char *) chunk, size);
            ESP32_USB_Serial.device->tx_blocking(chunk, size);
          }
        }
    }
}

static void ESP32SX_USB_fini()
{
    if (ESP32_USB_Serial.device) {
      ESP32_USB_Serial.device->close();
    }

    vTaskDelay(100);
    ESP_ERROR_CHECK(cdc_acm_host_uninstall());
    vTaskDelay(100);
    ESP_ERROR_CHECK(usb_host_uninstall());

    delete(USB_RX_FIFO);
    delete(USB_TX_FIFO);
}

static int ESP32SX_USB_available()
{
  int rval = 0;

  rval = USB_RX_FIFO->available();

  return rval;
}

static int ESP32SX_USB_read()
{
  int rval = -1;

  rval = USB_RX_FIFO->read();

  return rval;
}

static size_t ESP32SX_USB_write(const uint8_t *buffer, size_t size)
{
  size_t rval = size;

  rval = USB_TX_FIFO->write((char *) buffer,
                      (USB_TX_FIFO->room() > size ? size : USB_TX_FIFO->room()));

  return rval;
}

#elif ARDUINO_USB_CDC_ON_BOOT

#define USE_ASYNC_USB_OUTPUT
#define USBSerial                Serial

#if !ARDUINO_USB_MODE && defined(USE_ASYNC_USB_OUTPUT)
#define USB_MAX_WRITE_CHUNK_SIZE CONFIG_TINYUSB_CDC_TX_BUFSIZE

cbuf *USB_TX_FIFO;
#endif /* USE_ASYNC_USB_OUTPUT */

static void ESP32SX_USB_setup()
{
  USBSerial.setRxBufferSize(USB_RX_FIFO_SIZE);
#if ARDUINO_USB_MODE
  /* native CDC (HWCDC) */
  USBSerial.setTxBufferSize(USB_TX_FIFO_SIZE);
#elif defined(USE_ASYNC_USB_OUTPUT)
  USB_TX_FIFO = new cbuf(USB_TX_FIFO_SIZE);
#endif /* ARDUINO_USB_MODE */
}

static void ESP32SX_USB_loop()
{
#if !ARDUINO_USB_MODE && defined(USE_ASYNC_USB_OUTPUT)
  if (USBSerial)
  {
    uint8_t chunk[USB_MAX_WRITE_CHUNK_SIZE];

    size_t size = USBSerial.availableForWrite();
    size = (size > USB_MAX_WRITE_CHUNK_SIZE ? USB_MAX_WRITE_CHUNK_SIZE : size);
    size = (USB_TX_FIFO->available() < size ? USB_TX_FIFO->available() : size);

    USB_TX_FIFO->read((char *) chunk, size);
    USBSerial.write(chunk, size);
  }
#endif /* USE_ASYNC_USB_OUTPUT */
}

static void ESP32SX_USB_fini()
{
#if !ARDUINO_USB_MODE && defined(USE_ASYNC_USB_OUTPUT)
  delete(USB_TX_FIFO);
#endif /* USE_ASYNC_USB_OUTPUT */
}

static int ESP32SX_USB_available()
{
  int rval = 0;

  if (USBSerial) {
    rval = USBSerial.available();
  }

  return rval;
}

static int ESP32SX_USB_read()
{
  int rval = -1;

  if (USBSerial) {
    rval = USBSerial.read();
  }

  return rval;
}

static size_t ESP32SX_USB_write(const uint8_t *buffer, size_t size)
{
  size_t rval = size;

#if ARDUINO_USB_MODE
  /* Espressif native CDC (HWCDC) */
  if (USBSerial && (size < USBSerial.availableForWrite())) {
    rval = USBSerial.write(buffer, size);
  }
#else
  /* TinyUSB CDC (USBCDC) */
#if defined(USE_ASYNC_USB_OUTPUT)
  rval = USB_TX_FIFO->write((char *) buffer,
                      (USB_TX_FIFO->room() > size ? size : USB_TX_FIFO->room()));
#else
  if (USBSerial) {
    rval = USBSerial.write(buffer, size);
  }
#endif /* USE_ASYNC_USB_OUTPUT */
#endif /* ARDUINO_USB_MODE */

  return rval;
}
#endif /* USE_USB_HOST || ARDUINO_USB_CDC_ON_BOOT */

#if ARDUINO_USB_CDC_ON_BOOT || defined(USE_USB_HOST)
IODev_ops_t ESP32SX_USBSerial_ops = {
  "ESP32SX USB",
  ESP32SX_USB_setup,
  ESP32SX_USB_loop,
  ESP32SX_USB_fini,
  ESP32SX_USB_available,
  ESP32SX_USB_read,
  ESP32SX_USB_write
};
#endif /* USE_USB_HOST || ARDUINO_USB_CDC_ON_BOOT */
#endif /* CONFIG_IDF_TARGET_ESP32S2 */


const SoC_ops_t ESP32_ops = {
#if defined(CONFIG_IDF_TARGET_ESP32)
  SOC_ESP32,
  "ESP32",
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  SOC_ESP32S2,
  "ESP32-S2",
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  SOC_ESP32S3,
  "ESP32-S3",
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  SOC_ESP32C3,
  "ESP32-C3",
#else
#error "This ESP32 family build variant is not supported!"
#endif /* CONFIG_IDF_TARGET_ESP32-S2-S3-C3 */
  ESP32_setup,
  ESP32_post_init,
  ESP32_loop,
  ESP32_fini,
  ESP32_reset,
  ESP32_getChipId,
  ESP32_getResetInfoPtr,
  ESP32_getResetInfo,
  ESP32_getResetReason,
  ESP32_getFreeHeap,
  ESP32_random,
  ESP32_maxSketchSpace,
  ESP32_WiFi_set_param,
  ESP32_WiFi_transmit_UDP,
  ESP32_WiFiUDP_stopAll,
  ESP32_WiFi_hostname,
  ESP32_WiFi_clients_count,
  ESP32_EEPROM_begin,
  ESP32_EEPROM_extension,
  ESP32_SPI_begin,
  ESP32_swSer_begin,
  ESP32_swSer_enableRx,
#if !defined(CONFIG_IDF_TARGET_ESP32S2)
  &ESP32_Bluetooth_ops,
#else
  NULL,
#endif /* CONFIG_IDF_TARGET_ESP32S2 */
#if (defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)) && \
   (ARDUINO_USB_CDC_ON_BOOT || defined(USE_USB_HOST))
  &ESP32SX_USBSerial_ops,
#else
  NULL,
#endif /* USE_USB_HOST */
  NULL,
  ESP32_Display_setup,
  ESP32_Display_loop,
  ESP32_Display_fini,
  ESP32_Baro_setup,
  ESP32_WDT_setup,
  ESP32_WDT_fini,
};

#endif /* ESP32 */
