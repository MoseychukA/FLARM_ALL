#include <mutex>
#include "Arduino.h"
#include "pico/multicore.h"
#include "unit_conversions.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "comms.h"
#include "transponder_packet.h"
#include "packet_decoder.h"
#include "hal.h"
#include "unit_conversions.h"
#include "bsp.h"
#include "awb_utils.h"
#include "decode_utils.h" 
#include "aircraft_dictionary.h"
#include "nasa_cpr.h"
#include "adsbee.h"
#include "beast_tables.h"
#include "data_structures.h"
#include "hardware/watchdog.h"


 
BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});
SettingsManager settings_manager;
PacketDecoder decoder = PacketDecoder({ .enable_1090_error_correction = true });
AircraftDictionary send_dictionary;


// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)





void setup() 
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    comms_manager.Init();  //Сначала настроим вывод в КОМ порт
    sleep_ms(500);
    adsbee.Init();
    Serial.begin(115200);
    unsigned long t0 = millis(); while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(1000);
    Serial.print("Software ");
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
    comms_manager.console_printf(ver_soft.c_str());
    settings_manager.Load();    // Загрузить настройки по умолчанию. Нужно еще поработать с этой функцией.

    for (uint16_t i = 0; i < 4; i++)
    {
        adsbee.SetStatusLED(true);
        delay(200);
        adsbee.SetStatusLED(false);
        delay(200);
    }

    // Add a test aircraft to start.
    // Aircraft1090 test_aircraft;
    // test_aircraft.category = Aircraft1090::Category::kCategorySpaceTransatmosphericVehicle;
    // strcpy(test_aircraft.callsign, "TST1234");
    // test_aircraft.latitude_deg = 20;
    // test_aircraft.longitude_deg = 140;
    // test_aircraft.baro_altitude_ft = 10000;
    // test_aircraft.vertical_rate_fpm = -5;
    // test_aircraft.altitude_source = Aircraft1090::AltitudeSource::kAltitudeSourceBaro;
    // test_aircraft.direction_deg = 100;
    // test_aircraft.velocity_kts = 200;
    // adsbee.aircraft_dictionary.InsertAircraft(test_aircraft);

    // Устанавливаем WDT на 4 секунды:
    uint32_t timeout_ms = 4000;
    watchdog_enable(timeout_ms, /* pause_on_debug = */ false);

    Serial.println("WDT включён на 4 сек."); //WDT is on for 4 sec.

    Serial.println("Setup End\r\n");
    comms_manager.console_printf("\r\nSetup End\r\n");
}


void setup1()
{ 
    
}


void loop() 
{

  //  decoder.UpdateLogLoop();   // Вывод сырых пакетов ().
    comms_manager.Update();    // Вывод расшифрованных пакетов.
    adsbee.Update();
 
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) 
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

        txrx_test();
    }


    // Периодически сбрасываем WDT, иначе будет рестарт микроконтроллера
    watchdog_update();

}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 

}





float altitude1 = 100.0;
float altitude2 = 100.0;
float altitude3 = 100.0;
float altitude4 = 100.0;
float altitude5 = 100.0;


float speed1 = 300.0;
float speed2 = 300.0;
float speed3 = 300.0;
float speed4 = 300.0;
float speed5 = 300.0;

//bool alt_high0 = false;
bool alt_high1 = false;
bool alt_high2 = false;
bool alt_high3 = false;
bool alt_high4 = false;
bool alt_high5 = false;


float test_curse0 = 0.0;


//=============================== новый вариант расчета координат ================================
// Структура для хранения данных самолета
struct Aircraft_test {
    float latitude;
    float longitude;
    float course;
    float speed; // м/с
    double totalDistance; // общая дистанция для данного самолета
    double currentDistance; // текущая пройденная дистанция
    bool movingForward; // направление движения
    int id; // идентификатор самолета
};
// Константы


const float EARTH_RADIUS = 6371000.0; // Радиус Земли в метрах
const float DISTANCE_STEP = 250.0; // Шаг перемещения в метрах
const float TOTAL_DISTANCE = 10000.0; // Общая дистанция в метрах
const unsigned long UPDATE_INTERVAL = 1000; // Интервал обновления в мс


// Переменные для управления движением
float startLatitude = 55.958388;
float startLongitude = 37.243838;
float currentDistance = 0.0;
bool movingForward = true;
unsigned long lastUpdate = 0;

//// Функция перемещения самолета на заданное расстояние
//void moveAircraft(float distance)
//{
//    float lat1 = ThisAircraft.latitude * DEG_TO_RAD;
//    float lon1 = ThisAircraft.longitude * DEG_TO_RAD;
//    float bearing = ThisAircraft.course * DEG_TO_RAD;
//
//    float angular_distance = distance / EARTH_RADIUS;
//
//    // Вычисление новой широты
//    float lat2 = asin(sin(lat1) * cos(angular_distance) +
//        cos(lat1) * sin(angular_distance) * cos(bearing));
//
//    // Вычисление новой долготы
//    float dlon = atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
//        cos(angular_distance) - sin(lat1) * sin(lat2));
//
//    float lon2 = fmod(lon1 + dlon + 3 * PI, 2 * PI) - PI; // Нормализация долготы
//
//    // Обновление координат
//    ThisAircraft.latitude = lat2 * RAD_TO_DEG;
//    ThisAircraft.longitude = lon2 * RAD_TO_DEG;
//}


//
//// Функция вывода текущей позиции. Для теста
//void printCurrentPosition() {
//    Serial.printf("Дистанция: %.0f м | ", currentDistance);
//    Serial.printf("Координаты: %.6f°, %.6f° | ",
//        ThisAircraft.latitude, ThisAircraft.longitude);
//    Serial.printf("Курс: %.1f°\n", ThisAircraft.course);
//}

//// Функция проверки и изменения курса
//void checkAndUpdateCourse()
//{
//    if (movingForward && currentDistance >= TOTAL_DISTANCE)
//    {
//        // Достигли конечной точки - разворот на 180°
//        ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
//        movingForward = false;
//        currentDistance = 0.0;
//
//        //Serial.println(" ДОСТИГНУТА КОНЕЧНАЯ ТОЧКА ");
//        //Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);
//
//    }
//    else if (!movingForward && currentDistance >= TOTAL_DISTANCE)
//    {
//        // Вернулись к точке старта - снова разворот на 180°
//        ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
//        movingForward = true;
//        currentDistance = 0.0;
//
//        //Serial.println(" ВОЗВРАТ К ТОЧКЕ СТАРТА ");
//        //Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);
//    }
//}

// Функция вычисления расстояния между двумя точками(формула гаверсинуса)
float calculateDistance(float lat1, float lon1, float lat2, float lon2)
{
    float dLat = (lat2 - lat1) * DEG_TO_RAD;
    float dLon = (lon2 - lon1) * DEG_TO_RAD;

    float a = sin(dLat / 2) * sin(dLat / 2) +
        cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}


//--------------------------версия на 5 самолетов -----------------------------------------------
// Структура для хранения данных самолета
struct Aircraft5 {
    float latitude;
    float longitude;
    float course;
    float speed; // м/с
    float totalDistance; // общая дистанция для данного самолета
    float currentDistance; // текущая пройденная дистанция
    bool movingForward; // направление движения
    int id; // идентификатор самолета
};

// Массив из 5 самолетов
Aircraft5 aircraft5[5];

// Функция инициализации всех самолетов
void initializeAircraft() {
    // Самолет 1
    aircraft5[0].id = 1;
    aircraft5[0].latitude = startLatitude;
    aircraft5[0].longitude = startLongitude;
    aircraft5[0].course = 70.0;
    aircraft5[0].totalDistance = 10000.0;
    aircraft5[0].currentDistance = 0.0;
    aircraft5[0].movingForward = true;
    aircraft5[0].speed = 50.0;

    // Самолет 2
    aircraft5[1].id = 2;
    aircraft5[1].latitude = startLatitude;
    aircraft5[1].longitude = startLongitude;
    aircraft5[1].course = 120.0;
    aircraft5[1].totalDistance = 8000.0;
    aircraft5[1].currentDistance = 0.0;
    aircraft5[1].movingForward = true;
    aircraft5[1].speed = 45.0;

    // Самолет 3
    aircraft5[2].id = 3;
    aircraft5[2].latitude = startLatitude;
    aircraft5[2].longitude = startLongitude;
    aircraft5[2].course = 250.0;
    aircraft5[2].totalDistance = 9000.0;
    aircraft5[2].currentDistance = 0.0;
    aircraft5[2].movingForward = true;
    aircraft5[2].speed = 55.0;

    // Самолет 4
    aircraft5[3].id = 4;
    aircraft5[3].latitude = startLatitude;
    aircraft5[3].longitude = startLongitude;
    aircraft5[3].course = 290.0;
    aircraft5[3].totalDistance = 11000.0;
    aircraft5[3].currentDistance = 0.0;
    aircraft5[3].movingForward = true;
    aircraft5[3].speed = 40.0;

    // Самолет 5
    aircraft5[4].id = 5;
    aircraft5[4].latitude = startLatitude;
    aircraft5[4].longitude = startLongitude;
    aircraft5[4].course = 350.0;
    aircraft5[4].totalDistance = 11500.0;
    aircraft5[4].currentDistance = 0.0;
    aircraft5[4].movingForward = true;
    aircraft5[4].speed = 60.0;
}

// Функция инициализации всех самолетов синхронно
void initializeAircraftS() {
    // Самолет 1
    aircraft5[0].id = 1;
    aircraft5[0].latitude = startLatitude;
    aircraft5[0].longitude = startLongitude;
    aircraft5[0].course = 70.0;
    aircraft5[0].totalDistance = 10000.0;
    aircraft5[0].currentDistance = 0.0;
    aircraft5[0].movingForward = true;
    aircraft5[0].speed = 50.0;

    // Самолет 2
    aircraft5[1].id = 2;
    aircraft5[1].latitude = startLatitude;
    aircraft5[1].longitude = startLongitude;
    aircraft5[1].course = 120.0;
    aircraft5[1].totalDistance = 10000.0;
    aircraft5[1].currentDistance = 0.0;
    aircraft5[1].movingForward = true;
    aircraft5[1].speed = 45.0;

    // Самолет 3
    aircraft5[2].id = 3;
    aircraft5[2].latitude = startLatitude;
    aircraft5[2].longitude = startLongitude;
    aircraft5[2].course = 250.0;
    aircraft5[2].totalDistance = 10000.0;
    aircraft5[2].currentDistance = 0.0;
    aircraft5[2].movingForward = true;
    aircraft5[2].speed = 55.0;

    // Самолет 4
    aircraft5[3].id = 4;
    aircraft5[3].latitude = startLatitude;
    aircraft5[3].longitude = startLongitude;
    aircraft5[3].course = 290.0;
    aircraft5[3].totalDistance = 10000.0;
    aircraft5[3].currentDistance = 0.0;
    aircraft5[3].movingForward = true;
    aircraft5[3].speed = 40.0;

    // Самолет 5
    aircraft5[4].id = 5;
    aircraft5[4].latitude = startLatitude;
    aircraft5[4].longitude = startLongitude;
    aircraft5[4].course = 350.0;
    aircraft5[4].totalDistance = 10000.0;
    aircraft5[4].currentDistance = 0.0;
    aircraft5[4].movingForward = true;
    aircraft5[4].speed = 60.0;
}


// Функция перемещения конкретного самолета на заданное расстояние
void moveAircraft5(int aircraftIndex, float distance)
{
    float lat1 = aircraft5[aircraftIndex].latitude * DEG_TO_RAD;
    float lon1 = aircraft5[aircraftIndex].longitude * DEG_TO_RAD;
    float bearing = aircraft5[aircraftIndex].course * DEG_TO_RAD;

    float angular_distance = distance / EARTH_RADIUS;

    // Вычисление новой широты
    float lat2 = asin(sin(lat1) * cos(angular_distance) +
        cos(lat1) * sin(angular_distance) * cos(bearing));

    // Вычисление новой долготы
    float dlon = atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
        cos(angular_distance) - sin(lat1) * sin(lat2));

    float lon2 = fmod(lon1 + dlon + 3 * PI, 2 * PI) - PI; // Нормализация долготы

    // Обновление координат
    aircraft5[aircraftIndex].latitude = lat2 * RAD_TO_DEG;
    aircraft5[aircraftIndex].longitude = lon2 * RAD_TO_DEG;
}

// Функция проверки и изменения курса для конкретного самолета
void checkAndUpdateCourse5(int aircraftIndex)
{
    if (aircraft5[aircraftIndex].currentDistance >= aircraft5[aircraftIndex].totalDistance) {
        // Достигли конечной точки - разворот на 180°
        aircraft5[aircraftIndex].course = fmod(aircraft5[aircraftIndex].course + 180.0, 360.0);
        aircraft5[aircraftIndex].currentDistance = 0.0;

        // Переключение направления движения
        aircraft5[aircraftIndex].movingForward = !aircraft5[aircraftIndex].movingForward;

        //Serial.printf(" САМОЛЕТ %d: РАЗВОРОТ НА 180° | НОВЫЙ КУРС: %.1f° \n",
        //    aircraft5[aircraftIndex].id, aircraft5[aircraftIndex].course);
    }
}

// Функция вывода информации о всех самолетах
void printAllAircraftInfo5()
{
    Serial.println("Начальные параметры самолетов:");
    for (int i = 0; i < 5; i++) {
        Serial.printf("Самолет %d: Курс=%.1f°, Дистанция=%.0fм, Скорость=%.1fм/с\n",
            aircraft5[i].id, aircraft5[i].course, aircraft5[i].totalDistance, aircraft5[i].speed);
    }
    Serial.printf("Стартовые координаты для всех: %.6f°, %.6f°\n", startLatitude, startLongitude);
    Serial.println();
}

// Функция вывода текущих позиций всех самолетов
void printAllCurrentPositions5()
{
    for (int i = 0; i < 5; i++) {
        Serial.printf("Самолет %d | Расстояние: %.0f/%.0fм | Координаты: %.6f°, %.6f° | Курс: %.1f°\n",
            aircraft5[i].id,
            aircraft5[i].currentDistance,
            aircraft5[i].totalDistance,
            aircraft5[i].latitude,
            aircraft5[i].longitude,
            aircraft5[i].course);
    }
}

// Функция вычисления расстояния между двумя точками (формула гаверсинуса)
float calculateDistance5(float lat1, float lon1, float lat2, float lon2)
{
    float dLat = (lat2 - lat1) * DEG_TO_RAD;
    float dLon = (lon2 - lon1) * DEG_TO_RAD;

    float a = sin(dLat / 2) * sin(dLat / 2) +
        cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}

// Функция для получения информации о конкретном самолете
void getAircraftInfo5(int aircraftIndex)
{
    if (aircraftIndex >= 0 && aircraftIndex < 5)
    {
        Serial.printf("Информация о самолете %d:\n", aircraft5[aircraftIndex].id);
        Serial.printf("  Координаты: %.6f°, %.6f°\n",
            aircraft5[aircraftIndex].latitude, aircraft5[aircraftIndex].longitude);
        Serial.printf("  Курс: %.1f°\n", aircraft5[aircraftIndex].course);
        Serial.printf("  Пройденное расстояние: %.0f из %.0f метров\n",
            aircraft5[aircraftIndex].currentDistance, aircraft5[aircraftIndex].totalDistance);
        Serial.printf("  Направление: %s\n",
            aircraft5[aircraftIndex].movingForward ? "Вперед" : "Назад");
    }
}


bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

//================================================================================================

void txrx_test()
{

    if (set_test_coordinate5 == false)
    {
 
            initializeAircraftS();

        // Вывод начальных параметров всех самолетов
        //printAllAircraftInfo5();

        //Serial.println("Начало движения всех самолетов...\n");

        set_test_coordinate5 = true;
    }

 
    //====================================================================================================

    // Обновление позиции каждого самолета
    for (int i = 0; i < 5; i++)
    {
        moveAircraft5(i, DISTANCE_STEP);
        aircraft5[i].currentDistance += DISTANCE_STEP;
        checkAndUpdateCourse5(i);
    }

    // Вывод текущих позиций всех самолетов
   // printAllCurrentPositions5();
    Serial.println("----------------------------------------");

    //================ Самолет №1 ================================
    watchdog_update();
    if (!alt_high1)
    {
        altitude1 += 50.0;
        if (altitude1 > 1200.0)
        {
            altitude1 = 1200.0;
            alt_high1 = true;
        }
    }
    if (alt_high1)
    {

        altitude1 -= 50.0;
        if (altitude1 < 50.0)
        {
            altitude1 = 50.0;
            alt_high1 = false;
        }
    }
    speed1 -= 30.0;
    if (speed1 <= 30.0)
        speed1 = 1020.0;

       // Add a test aircraft to start.
     Aircraft1090 test_aircraft1;
     test_aircraft1.icao_address = 0x151DC1;
     test_aircraft1.category = Aircraft1090::Category::kCategorySpaceTransatmosphericVehicle;
     test_aircraft1.squawk = 1231;
     strcpy(test_aircraft1.callsign, "TST1231");
     test_aircraft1.latitude_deg = aircraft5[0].latitude;;
     test_aircraft1.longitude_deg = aircraft5[0].longitude;
     test_aircraft1.baro_altitude_ft = altitude1;
     test_aircraft1.vertical_rate_fpm = -50;
     test_aircraft1.altitude_source = Aircraft1090::AltitudeSource::kAltitudeSourceBaro;
     test_aircraft1.direction_deg = aircraft5[0].course;
     test_aircraft1.velocity_kts = speed1;
     adsbee.aircraft_dictionary.InsertAircraft(test_aircraft1);

    //======================== Самолет №2 ================================================

    //    aircraft5[1].latitude,
    //    aircraft5[1].longitude,
    //    aircraft5[1].course);
    watchdog_update();
    if (!alt_high2)
    {
        altitude2 += 50.0;
        if (altitude2 > 1200.0)
        {
            altitude2 = 1200.0;
            alt_high2 = true;
        }
    }
    if (alt_high2)
    {

        altitude2 -= 50.0;
        if (altitude2 < 50.0)
        {
            altitude2 = 50.0;
            alt_high2 = false;
        }
    }
    speed2 -= 30.0;
    if (speed2 <= 30.0)
        speed2 = 1020.0;

    // Add a test aircraft to start.
    Aircraft1090 test_aircraft2;
    test_aircraft2.icao_address = 0x151DC2;
    test_aircraft2.category = Aircraft1090::Category::kCategorySpaceTransatmosphericVehicle;
    test_aircraft2.squawk = 1232;
    strcpy(test_aircraft2.callsign, "TST1232");
    test_aircraft2.latitude_deg = aircraft5[1].latitude;;
    test_aircraft2.longitude_deg = aircraft5[1].longitude;
    test_aircraft2.baro_altitude_ft = altitude2;
    test_aircraft2.vertical_rate_fpm = 100;
    test_aircraft2.altitude_source = Aircraft1090::AltitudeSource::kAltitudeSourceBaro;
    test_aircraft2.direction_deg = aircraft5[1].course;
    test_aircraft2.velocity_kts = speed2;
    adsbee.aircraft_dictionary.InsertAircraft(test_aircraft2);
    //******************************************************************************************************************

    //================ Самолет №3 ================================

    //    aircraft5[2].latitude,
    //    aircraft5[2].longitude,
    //    aircraft5[2].course);
    watchdog_update();

    if (!alt_high3)
    {
        altitude3 += 40.0;
        if (altitude3 > 1000.0)
        {
            altitude3 = 1000.0;
            alt_high3 = true;
        }
    }
    if (alt_high3)
    {

        altitude3 -= 40.0;
        if (altitude3 < 50.0)
        {
            altitude3 = 50.0;
            alt_high3 = false;
        }
    }
    speed3 -= 30.0;
    if (speed3 <= 30.0)
        speed3 = 990.0;

    // Add a test aircraft to start.
    Aircraft1090 test_aircraft3;
    test_aircraft3.icao_address = 0x151DC3;
    test_aircraft3.category = Aircraft1090::Category::kCategorySpaceTransatmosphericVehicle;
    test_aircraft3.squawk = 1233;
    strcpy(test_aircraft3.callsign, "TST1233");
    test_aircraft3.latitude_deg = aircraft5[2].latitude;;
    test_aircraft3.longitude_deg = aircraft5[2].longitude;
    test_aircraft3.baro_altitude_ft = altitude3;
    test_aircraft3.vertical_rate_fpm = 150;
    test_aircraft3.altitude_source = Aircraft1090::AltitudeSource::kAltitudeSourceBaro;
    test_aircraft3.direction_deg = aircraft5[2].course;
    test_aircraft3.velocity_kts = speed3;
    adsbee.aircraft_dictionary.InsertAircraft(test_aircraft3);
    //======================== Самолет №4 ================================================

    //    aircraft5[3].latitude,
    //    aircraft5[3].longitude,
    //    aircraft5[3].course);
    watchdog_update();

    if (!alt_high4)
    {
        altitude4 += 50.0;
        if (altitude4 > 800.0)
        {
            altitude4 = 800.0;
            alt_high4 = true;
        }
    }
    if (alt_high4)
    {

        altitude4 -= 50.0;
        if (altitude4 < 50.0)
        {
            altitude4 = 50.0;
            alt_high4 = false;
        }
    }
    speed4 -= 30.0;
    if (speed4 <= 30.0)
        speed4 = 700.0;

    // Add a test aircraft to start.
    Aircraft1090 test_aircraft4;
    test_aircraft4.icao_address = 0x151DC4;
    test_aircraft4.category = Aircraft1090::Category::kCategorySpaceTransatmosphericVehicle;
    test_aircraft4.squawk = 1234;
    strcpy(test_aircraft4.callsign, "TST1234");
    test_aircraft4.latitude_deg = aircraft5[3].latitude;;
    test_aircraft4.longitude_deg = aircraft5[3].longitude;
    test_aircraft4.baro_altitude_ft = altitude4;
    test_aircraft4.vertical_rate_fpm = 200;
    test_aircraft4.altitude_source = Aircraft1090::AltitudeSource::kAltitudeSourceBaro;
    test_aircraft4.direction_deg = aircraft5[3].course;
    test_aircraft4.velocity_kts = speed4;
    adsbee.aircraft_dictionary.InsertAircraft(test_aircraft4);
    //******************************************************************************************************************


    //    aircraft5[4].latitude,
    //    aircraft5[4].longitude,
    //    aircraft5[4].course);
    watchdog_update();

    if (!alt_high5)
    {
        altitude5 += 50.0;
        if (altitude5 > 800.0)
        {
            altitude5 = 800.0;
            alt_high5 = true;
        }
    }
    if (alt_high5)
    {

        altitude5 -= 50.0;
        if (altitude5 < 50.0)
        {
            altitude5 = 50.0;
            alt_high5 = false;
        }
    }
    speed5 -= 30.0;
    if (speed5 <= 30.0)
        speed5 = 700.0;

    // Add a test aircraft to start.
    Aircraft1090 test_aircraft5;
    test_aircraft5.icao_address = 0x151DC5;
    test_aircraft5.category = Aircraft1090::Category::kCategorySpaceTransatmosphericVehicle;
    test_aircraft5.squawk = 1235;
    strcpy(test_aircraft5.callsign, "TST1235");
    test_aircraft5.latitude_deg = aircraft5[4].latitude;;
    test_aircraft5.longitude_deg = aircraft5[4].longitude;
    test_aircraft5.baro_altitude_ft = altitude5;
    test_aircraft5.vertical_rate_fpm = 250;
    test_aircraft5.altitude_source = Aircraft1090::AltitudeSource::kAltitudeSourceBaro;
    test_aircraft5.direction_deg = aircraft5[4].course;
    test_aircraft5.velocity_kts = speed5;
    adsbee.aircraft_dictionary.InsertAircraft(test_aircraft5);


    //******************************************************************************************************************

}

