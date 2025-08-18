//Программа имитации движения 5 самолетов на ESP32:


#include <Arduino.h> 
#include <WiFi.h>
#include <math.h>

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

// Константы
const float EARTH_RADIUS = 6371000.0; // Радиус Земли в метрах
//const float DEG_TO_RAD = PI / 180.0;
//const float RAD_TO_DEG = 180.0 / PI;
const float DISTANCE_STEP = 500.0; // Шаг перемещения в метрах
const unsigned long UPDATE_INTERVAL = 1000; // Интервал обновления в мс

// Переменные для управления движением
float startLatitude5 = 55.958388;
float startLongitude5 = 37.243838;
unsigned long lastUpdate5 = 0;

void setup() 
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("Инициализация системы имитации движения 5 самолетов");
  Serial.println("====================================================");

  // Инициализация самолетов
  initializeAircraft();

  // Вывод начальных параметров всех самолетов
  printAllAircraftInfo5();

  Serial.println("Начало движения всех самолетов...\n");
}

void loop() {
  unsigned long currentTime = millis();

  // Обновление позиции всех самолетов каждые UPDATE_INTERVAL миллисекунд
  if (currentTime - lastUpdate5 >= UPDATE_INTERVAL) {
    lastUpdate5 = currentTime;

    // Обновление позиции каждого самолета
    for (int i = 0; i < 5; i++) 
	{
      moveAircraft5(i, DISTANCE_STEP);
      aircraft5[i].currentDistance += DISTANCE_STEP;
      checkAndUpdateCourse5(i);
    }

    // Вывод текущих позиций всех самолетов
    printAllCurrentPositions5();
    Serial.println("----------------------------------------");
  }
}

// Функция инициализации всех самолетов
void initializeAircraft() {
  // Самолет 1
  aircraft5[0].id = 1;
  aircraft5[0].latitude = startLatitude5;
  aircraft5[0].longitude = startLongitude5;
  aircraft5[0].course = 70.0;
  aircraft5[0].totalDistance = 10000.0;
  aircraft5[0].currentDistance = 0.0;
  aircraft5[0].movingForward = true;
  aircraft5[0].speed = 50.0;

  // Самолет 2
  aircraft5[1].id = 2;
  aircraft5[1].latitude = startLatitude5;
  aircraft5[1].longitude = startLongitude5;
  aircraft5[1].course = 100.0;
  aircraft5[1].totalDistance = 8000.0;
  aircraft5[1].currentDistance = 0.0;
  aircraft5[1].movingForward = true;
  aircraft5[1].speed = 45.0;

  // Самолет 3
  aircraft5[2].id = 3;
  aircraft5[2].latitude = startLatitude5;
  aircraft5[2].longitude = startLongitude5;
  aircraft5[2].course = 250.0;
  aircraft5[2].totalDistance = 9000.0;
  aircraft5[2].currentDistance = 0.0;
  aircraft5[2].movingForward = true;
  aircraft5[2].speed = 55.0;

  // Самолет 4
  aircraft5[3].id = 4;
  aircraft5[3].latitude = startLatitude5;
  aircraft5[3].longitude = startLongitude5;
  aircraft5[3].course = 275.0;
  aircraft5[3].totalDistance = 11000.0;
  aircraft5[3].currentDistance = 0.0;
  aircraft5[3].movingForward = true;
  aircraft5[3].speed = 40.0;

  // Самолет 5
  aircraft5[4].id = 5;
  aircraft5[4].latitude = startLatitude5;
  aircraft5[4].longitude = startLongitude5;
  aircraft5[4].course = 350.0;
  aircraft5[4].totalDistance = 11000.0;
  aircraft5[4].currentDistance = 0.0;
  aircraft5[4].movingForward = true;
  aircraft5[4].speed = 60.0;
}

// Функция перемещения конкретного самолета на заданное расстояние
void moveAircraft5(int aircraftIndex, float distance) {
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

    Serial.printf(" САМОЛЕТ %d: РАЗВОРОТ НА 180° | НОВЫЙ КУРС: %.1f° \n",
                  aircraft5[aircraftIndex].id, aircraft5[aircraftIndex].course);
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
  Serial.printf("Стартовые координаты для всех: %.6f°, %.6f°\n", startLatitude5, startLongitude5);
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

/*
Особенности программы:

Структура данных:
Массив aircraft5[5] - содержит данные всех 5 самолетов
Каждый самолет имеет свои параметры: координаты, курс, дистанцию, ID

Параметры самолетов:
Самолет 1: Курс 70°, дистанция 10000м
Самолет 2: Курс 100°, дистанция 8000м
Самолет 3: Курс 250°, дистанция 9000м
Самолет 4: Курс 275°, дистанция 11000м
Самолет 5: Курс 350°, дистанция 11000м

Функциональность:
initializeAircraft() - инициализация всех самолетов
moveAircraft() - перемещение конкретного самолета
checkAndUpdateCourse() - проверка и смена курса для каждого самолета
printAllCurrentPositions() - вывод позиций всех самолетов

Вывод данных:
Программа выводит информацию о всех 5 самолетах каждую секунду:
Номер самолета
Пройденное/общее расстояние
Текущие координаты
Текущий курс
Уведомления о разворотах

Все самолеты движутся независимо друг от друга с разными параметрами.
*/