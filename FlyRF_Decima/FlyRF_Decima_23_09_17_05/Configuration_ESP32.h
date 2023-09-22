#pragma once

#define SOFTWARE_VERSION "ver.2023.09.17"  

//--------------------------------------------------------------------------------------------------------------------------------
// настройки Serial. Настройки для общения через UART
//--------------------------------------------------------------------------------------------------------------------------------
#define CONFIG_SERIAL                  Serial // какой Serial использовать для работы с программой UROVConfig.exe
#define CONFIG_SERIAL_SPEED            115200 // скорость работы с CONFIG_SERIAL
//--------------------------------------------------------------------------------------------------------------------------------
#define _DEBUG // закомментировать для выключения отладочной информации
//--------------------------------------------------------------------------------------------------------------------------------


#define LILYGO_TBeam_V1_X                // Модель модуля

#define LoRa_frequency      868.8E5      // Частота России
#define LoRa_test                        // Только для тестирования модуля LoRa
#define UNUSE_PIN                   (0)  // pin не назначен

#if defined(LILYGO_TBeam_V1_X)

#define GPS_RX_PIN                  34
#define GPS_TX_PIN                  12
#define BUTTON_PIN                  38
#define BUTTON_PIN_MASK             GPIO_SEL_38
#define I2C_SDA                     21
#define I2C_SCL                     22
#define PMU_IRQ                     35

#define RADIO_SCLK_PIN               5
#define RADIO_MISO_PIN              19
#define RADIO_MOSI_PIN              27
#define RADIO_CS_PIN                18
#define RADIO_DIO0_PIN              26
#define RADIO_RST_PIN               23
#define RADIO_DIO1_PIN              33
#define RADIO_BUSY_PIN              32

#define BOARD_LED                   4
#define LED_ON                      LOW
#define LED_OFF                     HIGH

#define GPS_BAUD_RATE               9600
#define HAS_GPS
#define GPS_TIME_PULSE              37  // Импульсы с модуля GPS

#define HAS_DISPLAY                     //Optional, bring your own board, no OLED !!
#define LCD_Led                     13  // Управление подсветкой дисплея

#define HAS_PMU                         // Подключение контроллера питания
#define XPOWERS_CHIP_AXP2101
#define IRQ_CHIP_AXP2101            35  // Прерывание с контроллера питания

#else
#error "For the first use, please define the board version and model in <utilities. h>"
#endif


//--------------------------------------------------------------------------------------------------------------------------------
// экраны
//--------------------------------------------------------------------------------------------------------------------------------
#define USE_TFT_MODULE // закомментировать, если не нужна поддержка TFT


//--------------------------------------------------------------------------------------------------------------------------------
// настройки TFT (используется экран с разрешением 320x240)
//--------------------------------------------------------------------------------------------------------------------------------
// цвета для TFT
// цвета для кнопок: цвет текста, не выяснил ,цвет рамки не нажат, цвет рамки при нажатии ,цвет заполнения
//--------------------------------------------------------------------------------------------------------------------------------
#define TFT_BACK_COLOR        TFT_BLACK           // цвет фона
#define TFT_BUTTON_COLORS TFT_WHITE, 0x8410, WHITE, 0xF800, 0x001F
#define TFT_BUTTON_COLORS_BLUE TFT_WHITE, TFT_GREEN, TFT_WHITE, TFT_RED, TFT_BLUE

#define TFT_FONT_COLOR 0x4A69                   // цвет шрифта по умолчанию
#define TFT_CHANNELS_BUTTON_COLORS 0x3A8D, 0xC618, 0x8410, 0xF800, 0xEF7D // цвета кнопок для каналов
#define INFO_BOX_BACK_COLOR 0x6161              // цвет фона для информационного бокса
#define INFO_BOX_BORDER_COLOR TFT_BLACK         // цвет рамки информационного бокса
#define INFO_BOX_CAPTION_COLOR 0x33D6           // цвет заголовка информационного бокса
#define SENSOR_BOX_FONT_COLOR TFT_WHITE         // цвет показаний датчика
#define SENSOR_BOX_UNIT_COLOR TFT_WHITE         // цвет единиц изменений датчика
#define MODE_ON_COLOR 0x0400                    // цвет "вкл", "авто"
#define MODE_OFF_COLOR 0x8000                   // цвет "выкл", "ручной"
#define CHANNELS_BUTTONS_TEXT_COLOR TFT_WHITE   // цвет текста кнопок каналов
#define CHANNELS_BUTTONS_BG_COLOR 0xEF7D        // цвет фона кнопок каналов
#define CHANNEL_BUTTONS_TEXT_COLOR 0x3A8D       // цвет текста кнопки одного канала
#define TIME_PART_FONT_COLOR 0x0410             // цвет текста кнопки установки компонента времени
#define TIME_PART_SELECTED_FONT_COLOR TFT_WHITE // цвет текста активной кнопки компонента времени
#define TIME_PART_BG_COLOR 0xEF7D               // цвет фона кнопки компонента времени
#define TIME_PART_SELECTED_BG_COLOR 0x0400      // цвет фона выбранной кнопки компонента времени
#define STATUS_ON_COLOR 0x07E0                  // цвет статусов на экране ожидания
//--------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef _DEBUG
#define DBG(s) { Serial.print((s)); }
#define DBGLN(s) { Serial.print((s)); Serial.println(); }
#else
#define DBG(s) (void) 0
#define DBGLN(s) (void) 0
#endif
#define ENDL '\n'
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
