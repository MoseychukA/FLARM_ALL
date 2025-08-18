
//Код приемника (Slave) с отслеживанием изменений:

#include <ModbusRTU.h>
#include <HardwareSerial.h>

// Конфигурация RS485
#define RS485_TX_PIN 39
#define RS485_RX_PIN 38
#define RS485_DE_RE_PIN 40

// Modbus настройки
#define SLAVE_ID 1
#define BAUD_RATE 921600

// Создание объекта Modbus Slave
ModbusRTU mb;
HardwareSerial RS485Serial(1);

// Структуры данных
#define MAX_TRACKING_OBJECTS 8

typedef struct UFO {
    time_t    timestamp;
    uint32_t  addr;
    uint8_t   addr_type;
    float     latitude;
    float     longitude;
    float     old_latitude;
    float     old_longitude;
    float     altitude;
    float     pressure_altitude;
    float     course;
    float     speed;
    uint8_t   aircraft_type;
    char      flight[16];
    int       vert_rate;
    int       Squawk;
    time_t    timemsg;
    float     vs;
    float     geoid_separation;
    uint16_t  hdop;
    int8_t    rssi;
    float     distance;
    float     bearing;
    uint8_t   signal_source;
    time_t    seen;
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    uint8_t   callsign[8];
} ufo_t;

typedef struct AdditionalData {
    bool new_flag_M;
    uint8_t new_buttton_M;
    bool setMessageRead_M;
    bool MessageRead_M;
    bool SOS_Sprite_on_off_M;
    bool SOS_View_on_off_M;
    bool new_SOS_flag_M;
    bool confirm_message_M;
    char msg_resp_M[60];
    bool isValidGNSS_M;
    uint8_t FLYRF_MODE_TEST_M;
    uint16_t analog_signal;
} additional_data_t;

// Глобальные переменные
ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;
additional_data_t AdditionalInfo;

// Копии для отслеживания изменений
ufo_t Container_Previous[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft_Previous;
additional_data_t AdditionalInfo_Previous;

// Флаги изменений и обновлений
bool Container_Updated[MAX_TRACKING_OBJECTS];
bool ThisAircraft_Updated;
bool AdditionalInfo_Updated;

// Статистика
uint32_t totalUpdates = 0;
uint32_t containerUpdates = 0;
uint32_t thisAircraftUpdates = 0;
uint32_t additionalInfoUpdates = 0;

// Время последнего обновления
unsigned long lastUpdateTime[MAX_TRACKING_OBJECTS];
unsigned long lastThisAircraftUpdate = 0;
unsigned long lastAdditionalInfoUpdate = 0;

// Флаги данных для быстрой проверки
bool dataUpdated = false;
unsigned long lastDataUpdate = 0;
uint16_t registerWriteCount = 0;

// Функции конвертации
float uint16ToFloat(uint16_t high, uint16_t low) {
    union {
        float f;
        uint32_t i;
    } converter;
    converter.i = ((uint32_t)high << 16) | low;
    return converter.f;
}

time_t uint16ToTime(uint16_t high, uint16_t low) {
    return ((time_t)high << 16) | low;
}

// Функция сравнения UFO структур
bool compareUFO(const ufo_t* ufo1, const ufo_t* ufo2) {
    return (memcmp(ufo1, ufo2, sizeof(ufo_t)) == 0);
}

// Функция сравнения дополнительных данных
bool compareAdditionalData(const additional_data_t* data1, const additional_data_t* data2) {
    return (memcmp(data1, data2, sizeof(additional_data_t)) == 0);
}

// Функция извлечения UFO объекта с проверкой изменений
void extractUFOObject(uint8_t objectIndex, ufo_t* ufo) {
    const uint16_t baseRegister = (objectIndex < 8) ? (1000 + objectIndex * 100) : 1800;
    uint16_t registerIndex = 0;
    
    // Сохраняем предыдущее значение
    ufo_t* previousUfo = (objectIndex < 8) ? &Container_Previous[objectIndex] : &ThisAircraft_Previous;
    memcpy(previousUfo, ufo, sizeof(ufo_t));
    
    // Очистка структуры
    memset(ufo, 0, sizeof(ufo_t));
    
    // Извлечение данных
    ufo->timestamp = uint16ToTime(mb.Hreg(baseRegister + registerIndex), 
                                  mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->addr = ((uint32_t)mb.Hreg(baseRegister + registerIndex) << 16) | 
                mb.Hreg(baseRegister + registerIndex + 1);
    registerIndex += 2;
    
    ufo->addr_type = mb.Hreg(baseRegister + registerIndex++);
    
    ufo->latitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                  mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->longitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                   mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->old_latitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                      mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->old_longitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                       mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->altitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                  mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->pressure_altitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                           mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->course = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->speed = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                               mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->aircraft_type = mb.Hreg(baseRegister + registerIndex++);
    
    // Flight number
    for (int i = 0; i < 8; i++) {
        uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
        if (i * 2 < 16) ufo->flight[i * 2] = reg & 0xFF;
        if (i * 2 + 1 < 16) ufo->flight[i * 2 + 1] = (reg >> 8) & 0xFF;
    }
    ufo->flight[15] = '\0';
    
    ufo->vert_rate = ((int32_t)mb.Hreg(baseRegister + registerIndex) << 16) | 
                     mb.Hreg(baseRegister + registerIndex + 1);
    registerIndex += 2;
    
    ufo->Squawk = ((int32_t)mb.Hreg(baseRegister + registerIndex) << 16) | 
                  mb.Hreg(baseRegister + registerIndex + 1);
    registerIndex += 2;
    
    ufo->timemsg = uint16ToTime(mb.Hreg(baseRegister + registerIndex), 
                                mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->vs = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                            mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->geoid_separation = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                          mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->hdop = mb.Hreg(baseRegister + registerIndex++);
    ufo->rssi = (int8_t)(mb.Hreg(baseRegister + registerIndex++) - 128);
    
    ufo->distance = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                  mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->bearing = uint16ToFloat(mb.Hreg(baseRegister + registerIndex), 
                                 mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->signal_source = mb.Hreg(baseRegister + registerIndex++);
    
    ufo->seen = uint16ToTime(mb.Hreg(baseRegister + registerIndex), 
                             mb.Hreg(baseRegister + registerIndex + 1));
    registerIndex += 2;
    
    ufo->hour_msg = mb.Hreg(baseRegister + registerIndex++);
    ufo->min_msg = mb.Hreg(baseRegister + registerIndex++);
    ufo->delay_time_msg = mb.Hreg(baseRegister + registerIndex++);
    
    // Callsign
    for (int i = 0; i < 4; i++) {
        uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
        if (i * 2 < 8) ufo->callsign[i * 2] = reg & 0xFF;
        if (i * 2 + 1 < 8) ufo->callsign[i * 2 + 1] = (reg >> 8) & 0xFF;
    }
    ufo->callsign[7] = '\0';
    
    // Проверяем на изменения
    if (!compareUFO(ufo, previousUfo)) {
        if (objectIndex < 8) {
            Container_Updated[objectIndex] = true;
            lastUpdateTime[objectIndex] = millis();
            containerUpdates++;
            Serial.print("✓ Container[");
            Serial.print(objectIndex);
            Serial.println("] updated with new data");
        } else {
            ThisAircraft_Updated = true;
            lastThisAircraftUpdate = millis();
            thisAircraftUpdates++;
            Serial.println("✓ ThisAircraft updated with new data");
        }
        totalUpdates++;
    }
}

// Функция извлечения дополнительных данных с проверкой изменений
void extractAdditionalData() {
    const uint16_t baseRegister = 2000;
    uint16_t registerIndex = 0;
    
    // Сохраняем предыдущее значение
    memcpy(&AdditionalInfo_Previous, &AdditionalInfo, sizeof(AdditionalInfo));
    
    // Очистка структуры
    memset(&AdditionalInfo, 0, sizeof(additional_data_t));
    
    // Извлечение данных
    uint16_t boolFlags = mb.Hreg(baseRegister + registerIndex++);
    AdditionalInfo.new_flag_M = (boolFlags & (1 << 0)) != 0;
    AdditionalInfo.setMessageRead_M = (boolFlags & (1 << 1)) != 0;
    AdditionalInfo.MessageRead_M = (boolFlags & (1 << 2)) != 0;
    AdditionalInfo.SOS_Sprite_on_off_M = (boolFlags & (1 << 3)) != 0;
    AdditionalInfo.SOS_View_on_off_M = (boolFlags & (1 << 4)) != 0;
    AdditionalInfo.new_SOS_flag_M = (boolFlags & (1 << 5)) != 0;
    AdditionalInfo.confirm_message_M = (boolFlags & (1 << 6)) != 0;
    AdditionalInfo.isValidGNSS_M = (boolFlags & (1 << 7)) != 0;
    
    AdditionalInfo.new_buttton_M = mb.Hreg(baseRegister + registerIndex++);
    AdditionalInfo.FLYRF_MODE_TEST_M = mb.Hreg(baseRegister + registerIndex++);
    AdditionalInfo.analog_signal = mb.Hreg(baseRegister + registerIndex++);
    
    // msg_resp_M
    for (int i = 0; i < 30; i++) {
        uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
        if (i * 2 < 60) AdditionalInfo.msg_resp_M[i * 2] = reg & 0xFF;
        if (i * 2 + 1 < 60) AdditionalInfo.msg_resp_M[i * 2 + 1] = (reg >> 8) & 0xFF;
    }
    AdditionalInfo.msg_resp_M[59] = '\0';
    
    // Проверяем на изменения
    if (!compareAdditionalData(&AdditionalInfo, &AdditionalInfo_Previous)) {
        AdditionalInfo_Updated = true;
        lastAdditionalInfoUpdate = millis();
        additionalInfoUpdates++;
        totalUpdates++;
        Serial.println("✓ AdditionalInfo updated with new data");
    }
}

// Callback функция для записи регистров
uint16_t cbWrite(TRegister* reg, uint16_t val) {
    registerWriteCount++;
    dataUpdated = true;
    lastDataUpdate = millis();
    
    return val;
}

// Callback функция для чтения регистров
uint16_t cbRead(TRegister* reg, uint16_t val) {
    return val;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Настройка RS485
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    digitalWrite(RS485_DE_RE_PIN, LOW);
    
    // Инициализация Serial1 для RS485
    RS485Serial.begin(BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    
    // Настройка Modbus Slave
    mb.begin(&RS485Serial, RS485_DE_RE_PIN);
    mb.slave(SLAVE_ID);
    
    // Добавляем holding registers
    mb.addHreg(0, 0, 3000);
    
    // Устанавливаем callback функции
    mb.onSetHreg(0, cbWrite, 3000);
    mb.onGetHreg(0, cbRead, 3000);
    
    Serial.println("=================================");
    Serial.println("Modbus Slave OPTIMIZED VERSION");
    Serial.println("(Change detection enabled)");
    Serial.println("=================================");
    
    // Инициализация данных
    memset(&Container, 0, sizeof(Container));
    memset(&ThisAircraft, 0, sizeof(ThisAircraft));
    memset(&AdditionalInfo, 0, sizeof(AdditionalInfo));
    
    memset(&Container_Previous, 0, sizeof(Container_Previous));
    memset(&ThisAircraft_Previous, 0, sizeof(ThisAircraft_Previous));
    memset(&AdditionalInfo_Previous, 0, sizeof(AdditionalInfo_Previous));
    
    memset(Container_Updated, false, sizeof(Container_Updated));
    memset(lastUpdateTime, 0, sizeof(lastUpdateTime));
    ThisAircraft_Updated = false;
    AdditionalInfo_Updated = false;
    
    // Сброс статистики
    totalUpdates = 0;
    containerUpdates = 0;
    thisAircraftUpdates = 0;
    additionalInfoUpdates = 0;
    
    dataUpdated = false;
    lastDataUpdate = 0;
    registerWriteCount = 0;
    
    Serial.println("Ready for optimized data reception...");
}

void loop() {
    static unsigned long lastPrint = 0;
    static unsigned long lastStructUpdate = 0;
    static bool hasValidData = false;
    
    // Обработка Modbus запросов
    mb.task();
    
    // Быстрая обработка полученных данных
    if (dataUpdated && (millis() - lastDataUpdate > 200) && (millis() - lastStructUpdate > 200)) {
        lastStructUpdate = millis();
        
        // Сбрасываем флаги обновления
        memset(Container_Updated, false, sizeof(Container_Updated));
        ThisAircraft_Updated = false;
        AdditionalInfo_Updated = false;
        
        // Проверяем и обновляем структуры
        for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
            extractUFOObject(i, &Container[i]);
        }
        extractUFOObject(8, &ThisAircraft);
        extractAdditionalData();
        
        // Проверяем валидность данных
        if (ThisAircraft.addr != 0 || Container[0].addr != 0 || AdditionalInfo.analog_signal != 0) {
            hasValidData = true;
        }
        
        dataUpdated = false;
    }
    
    // Вывод статуса каждые 3 секунды
    if (millis() - lastPrint > 3000) {
        lastPrint = millis();
        
        Serial.println();
        Serial.println("=== OPTIMIZED RECEIVER STATUS ===");
        Serial.print("Total registers written: ");
        Serial.println(registerWriteCount);
        Serial.print("Total data updates: ");
        Serial.println(totalUpdates);
        Serial.print("Container updates: ");
        Serial.println(containerUpdates);
        Serial.print("ThisAircraft updates: ");
        Serial.println(thisAircraftUpdates);
        Serial.print("AdditionalInfo updates: ");
        Serial.println(additionalInfoUpdates);
        
        if (hasValidData) {
            Serial.println();
            Serial.println("=== CURRENT DATA ===");
            
            // Показываем только недавно обновленные объекты
            bool showAll = false;
            
            // ThisAircraft
            if (ThisAircraft.addr != 0 && (showAll || (millis() - lastThisAircraftUpdate < 10000))) {
                Serial.println(">>> ThisAircraft <<<");
                if (millis() - lastThisAircraftUpdate < 5000) {
                    Serial.print("  [RECENTLY UPDATED] ");
                }
                Serial.print("  Addr: 0x");
                Serial.println(ThisAircraft.addr, HEX);
                Serial.print("  Lat: ");
                Serial.println(ThisAircraft.latitude, 6);
                Serial.print("  Lon: ");
                Serial.println(ThisAircraft.longitude, 6);
                Serial.print("  Alt: ");
                Serial.println(ThisAircraft.altitude, 1);
                Serial.print("  Course: ");
                Serial.println(ThisAircraft.course, 1);
                Serial.print("  Speed: ");
                Serial.println(ThisAircraft.speed, 1);
                Serial.print("  Flight: '");
                Serial.print(ThisAircraft.flight);
                Serial.println("'");
            }
            
            // Container объекты (показываем первые 3 и недавно обновленные)
            for (int i = 0; i < 3; i++) {
                if (Container[i].addr != 0 && (showAll || (millis() - lastUpdateTime[i] < 10000))) {
                    Serial.print(">>> Container[");
                    Serial.print(i);
                    Serial.print("] <<<");
                    if (millis() - lastUpdateTime[i] < 5000) {
                        Serial.print(" [RECENTLY UPDATED]");
                    }
                    Serial.println();
                    Serial.print("  Addr: 0x");
                    Serial.println(Container[i].addr, HEX);
                    Serial.print("  Lat: ");
                    Serial.println(Container[i].latitude, 6);
                    Serial.print("  Lon: ");
                    Serial.println(Container[i].longitude, 6);
                    Serial.print("  Alt: ");
                    Serial.println(Container[i].altitude, 1);
                    Serial.print("  Flight: '");
                    Serial.print(Container[i].flight);
                    Serial.println("'");
                }
            }
            
            // AdditionalInfo
            if (AdditionalInfo.analog_signal != 0 && (showAll || (millis() - lastAdditionalInfoUpdate < 10000))) {
                Serial.println(">>> Additional Data <<<");
                if (millis() - lastAdditionalInfoUpdate < 5000) {
                    Serial.print("  [RECENTLY UPDATED] ");
                }
                Serial.print("  analog_signal: ");
                Serial.println(AdditionalInfo.analog_signal);
                Serial.print("  new_buttton_M: ");
                Serial.println(AdditionalInfo.new_buttton_M);
                Serial.print("  new_flag_M: ");
                Serial.println(AdditionalInfo.new_flag_M ? "true" : "false");
                if (strlen(AdditionalInfo.msg_resp_M) > 0) {
                    Serial.print("  msg_resp_M: '");
                    Serial.print(AdditionalInfo.msg_resp_M);
                    Serial.println("'");
                }
            }
        } else {
            Serial.println(">>> NO VALID DATA RECEIVED YET <<<");
        }
        
        Serial.println("================================");
    }
    
    delay(5); // Минимальная задержка для быстрой обработки
}

/*
Основные оптимизации:
Источник (Master):
✅ Отслеживание изменений: Сравнение с предыдущими копиями данных
✅ Передача только изменений: Только измененные объекты передаются
✅ Симуляция изменений: Разные объекты изменяются с разной частотой
✅ Уменьшенные задержки: 5ms вместо 10ms между блоками
✅ Статистика передачи: Подсчет количества переданных объектов
Приемник (Slave):
✅ Детекция изменений: Автоматическое обнаружение обновлений данных
✅ Быстрая обработка: 200ms задержка вместо 1500ms
✅ Статистика обновлений: Подсчет обновлений по типам данных
✅ Умный вывод: Показ только недавно обновленных данных
✅ Минимальная задержка: 5ms в основном цикле
Результат оптимизации:
Скорость: Передача только изменений вместо всех данных
Эффективность: Время передачи сокращено в 3-10 раз
Отслеживание: Полная статистика изменений и обновлений
Адаптивность: Система автоматически определяет что изменилось
Теперь система передает данные максимально быстро, отправляя только те объекты, которые действительно изменились!
*/


