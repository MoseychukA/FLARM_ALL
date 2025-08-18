//Программа для получения и расшифровки пакета на ESP32S3 :


#include <HardwareSerial.h>

// Настройка Serial1 для ESP32S3
#define RX_PIN 42
#define TX_PIN 41
#define SERIAL_BAUD 115200  // Установите нужную скорость

// Структура для хранения данных самолета
struct AircraftData {
    uint32_t icao_address;      // ICAO (6 hex digits)
    uint16_t squawk;            // SQUAWK (4 octal digits)
    char callsign[32];          // CALL (строка)
    int32_t baro_altitude_ft;   // ALT_BARO (целое число)
    float velocity_kts;         // VELH (скорость в узлах)
    float direction_deg;        // TRACK (направление в градусах)
    double latitude_deg;        // LAT (широта, 5 знаков после запятой)
    double longitude_deg;       // LON (долгота, 5 знаков после запятой)
    int32_t vertical_rate_fpm;  // VELV (вертикальная скорость)
};

// Буфер для приема данных
char receive_buffer[256];
int buffer_index = 0;

void setup() {
    // Инициализация основного Serial для отладки
    Serial.begin(115200);
    while (!Serial) delay(10);

    // Инициализация Serial1 для приема данных
    Serial1.begin(SERIAL_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

    Serial.println("ESP32S3 AircraftData Receiver Started");
    Serial.println("Waiting for data...");

    // Очистка буфера
    memset(receive_buffer, 0, sizeof(receive_buffer));
}

void loop() {
    // Чтение данных из Serial1
    while (Serial1.available()) 
    {
        char received_char = Serial1.read();

        // Проверка на переполнение буфера
        if (buffer_index >= sizeof(receive_buffer) - 1) 
        {
            Serial.println("Buffer overflow! Resetting...");
            buffer_index = 0;
            memset(receive_buffer, 0, sizeof(receive_buffer));
            continue;
        }

        // Добавление символа в буфер
        receive_buffer[buffer_index] = received_char;
        buffer_index++;

        // Проверка на конец сообщения (\r\n)
        if (received_char == '\n' && buffer_index > 1 && receive_buffer[buffer_index - 2] == '\r') 
        {
            // Завершение строки
            receive_buffer[buffer_index] = '\0';

            // Обработка полученного пакета
            processReceivedPacket(receive_buffer);

            // Сброс буфера для следующего пакета
            buffer_index = 0;
            memset(receive_buffer, 0, sizeof(receive_buffer));
        }
    }
}

void processReceivedPacket(const char* packet) 
{
    Serial.print("Received packet: ");
    Serial.println(packet);

    AircraftData aircraft;

    // Парсинг данных согласно формату
    // Формат: ICAO,SQUAWK,CALL,ALT_BARO,VELH,TRACK,LAT,LON,VELV

    char temp_callsign[32];
    int parsed_fields = sscanf(packet,
        "%6X,%4o,%31[^,],%d,%f,%f,%lf,%lf,%d",
        &aircraft.icao_address,
        &aircraft.squawk,
        temp_callsign,
        &aircraft.baro_altitude_ft,
        &aircraft.velocity_kts,
        &aircraft.direction_deg,
        &aircraft.latitude_deg,
        &aircraft.longitude_deg,
        &aircraft.vertical_rate_fpm
    );

    // Копирование callsign
    strncpy(aircraft.callsign, temp_callsign, sizeof(aircraft.callsign) - 1);
    aircraft.callsign[sizeof(aircraft.callsign) - 1] = '\0';

    // Проверка успешности парсинга
    if (parsed_fields == 9) 
    {
        Serial.println("✓ Packet parsed successfully:");
        printAircraftData(aircraft);
    }
    else 
    {
        Serial.print("✗ Parsing failed! Parsed fields: ");
        Serial.println(parsed_fields);
        Serial.println("Raw packet data:");
        Serial.println(packet);
    }
}

void printAircraftData(const AircraftData& aircraft) {
    Serial.println("--- Aircraft Data ---");
    Serial.printf("ICAO Address: %06X\n", aircraft.icao_address);
    Serial.printf("Squawk: %04o \n", aircraft.squawk);
    Serial.printf("Callsign: %s\n", aircraft.callsign);
    Serial.printf("Barometric Altitude: %d ft\n", aircraft.baro_altitude_ft);
    Serial.printf("Velocity: %.0f kts\n", aircraft.velocity_kts);
    Serial.printf("Track: %.0f°\n", aircraft.direction_deg);
    Serial.printf("Latitude: %.5f°\n", aircraft.latitude_deg);
    Serial.printf("Longitude: %.5f°\n", aircraft.longitude_deg);
    Serial.printf("Vertical Rate: %d fpm\n", aircraft.vertical_rate_fpm);
    Serial.println("---------------------\n");
}

//// Дополнительная функция для получения структуры данных
//AircraftData getLastAircraftData() 
//{
//    // Эта функция может быть использована для получения последних данных
//    // из других частей программы
//    static AircraftData last_aircraft_data = { 0 };
//    return last_aircraft_data;
//}
//
//// Функция для проверки валидности данных
//bool isValidAircraftData(const AircraftData& aircraft) 
//{
//    // Проверка основных параметров
//    if (aircraft.icao_address == 0) return false;
//    if (aircraft.latitude_deg < -90 || aircraft.latitude_deg > 90) return false;
//    if (aircraft.longitude_deg < -180 || aircraft.longitude_deg > 180) return false;
//    if (strlen(aircraft.callsign) == 0) return false;
//
//    return true;
//}


//Дополнительные функции для обработки ошибок :

//
//// Функция для очистки буфера Serial1
//void clearSerialBuffer() 
//{
//    while (Serial1.available()) {
//        Serial1.read();
//    }
//}
//
//// Функция для проверки контрольной суммы (если используется)
//bool validateChecksum(const char* packet) 
//{
//    // Реализация проверки CRC если необходимо
//    return true; // placeholder
//}
//
