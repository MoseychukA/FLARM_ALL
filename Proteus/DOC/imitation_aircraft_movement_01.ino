Вот программа для имитации движения самолета на ESP32:


#include <WiFi.h>
#include <math.h>

// Структура для хранения данных самолета
struct Aircraft {
  double latitude;
  double longitude;
  double course;
  double speed; // м/с
};

Aircraft ThisAircraft;

// Константы
const double EARTH_RADIUS = 6371000.0; // Радиус Земли в метрах
const double DEG_TO_RAD = PI / 180.0;
const double RAD_TO_DEG = 180.0 / PI;
const double DISTANCE_STEP = 500.0; // Шаг перемещения в метрах
const double TOTAL_DISTANCE = 10000.0; // Общая дистанция в метрах
const unsigned long UPDATE_INTERVAL = 1000; // Интервал обновления в мс

// Переменные для управления движением
double startLatitude, startLongitude;
double currentDistance = 0.0;
bool movingForward = true;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Инициализация системы имитации движения самолета");

  // Инициализация начальных координат
  ThisAircraft.latitude = 55.958388;
  ThisAircraft.longitude = 37.243838;
  ThisAircraft.course = 70.0;
  ThisAircraft.speed = 50.0; // 50 м/с (180 км/ч)

  // Сохранение начальных координат
  startLatitude = ThisAircraft.latitude;
  startLongitude = ThisAircraft.longitude;

  Serial.println("Начальные параметры:");
  Serial.printf("Широта: %.6f°\n", ThisAircraft.latitude);
  Serial.printf("Долгота: %.6f°\n", ThisAircraft.longitude);
  Serial.printf("Курс: %.1f°\n", ThisAircraft.course);
  Serial.printf("Скорость: %.1f м/с\n", ThisAircraft.speed);
  Serial.println("Начало движения...\n");
}

void loop() {
  unsigned long currentTime = millis();

  // Обновление позиции каждые UPDATE_INTERVAL миллисекунд
  if (currentTime - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = currentTime;

    // Движение на DISTANCE_STEP метров
    moveAircraft(DISTANCE_STEP);
    currentDistance += DISTANCE_STEP;

    // Вывод текущих координат
    printCurrentPosition();

    // Проверка достижения конечной точки или точки старта
    checkAndUpdateCourse();
  }
}

// Функция перемещения самолета на заданное расстояние
void moveAircraft(double distance) {
  double lat1 = ThisAircraft.latitude * DEG_TO_RAD;
  double lon1 = ThisAircraft.longitude * DEG_TO_RAD;
  double bearing = ThisAircraft.course * DEG_TO_RAD;

  // Вычисление новых координат по формулам сферической геометрии
  double lat2 = asin(sin(lat1) * cos(distance / EARTH_RADIUS) +
                     cos(lat1)  sin(distance / EARTH_RADIUS)  cos(bearing));

  double lon2 = lon1 + atan2(sin(bearing)  sin(distance / EARTH_RADIUS)  cos(lat1),
                            cos(distance / EARTH_RADIUS) - sin(lat1) * sin(lat2));

  // Обновление координат
  ThisAircraft.latitude = lat2 * RAD_TO_DEG;
  ThisAircraft.longitude = lon2 * RAD_TO_DEG;
}

// Функция вывода текущей позиции
void printCurrentPosition() {
  Serial.printf("Дистанция: %.0f м | ", currentDistance);
  Serial.printf("Координаты: %.6f°, %.6f° | ",
                ThisAircraft.latitude, ThisAircraft.longitude);
  Serial.printf("Курс: %.1f°\n", ThisAircraft.course);
}

// Функция проверки и изменения курса
void checkAndUpdateCourse() {
  if (movingForward && currentDistance >= TOTAL_DISTANCE) {
    // Достигли конечной точки - разворот на 180°
    ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
    movingForward = false;
    currentDistance = 0.0;

    Serial.println(" ДОСТИГНУТА КОНЕЧНАЯ ТОЧКА ");
    Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);

  } else if (!movingForward && currentDistance >= TOTAL_DISTANCE) {
    // Вернулись к точке старта - снова разворот на 180°
    ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
    movingForward = true;
    currentDistance = 0.0;

    Serial.println(" ВОЗВРАТ К ТОЧКЕ СТАРТА ");
    Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);
  }
}

// Функция вычисления расстояния между двумя точками (формула гаверсинуса)
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
  double dLat = (lat2 - lat1) * DEG_TO_RAD;
  double dLon = (lon2 - lon1) * DEG_TO_RAD;

  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(lat1  DEG_TO_RAD)  cos(lat2  DEG_TO_RAD)
             sin(dLon / 2) * sin(dLon / 2);

  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return EARTH_RADIUS * c;
}

// Дополнительная функция для вывода общей информации о маршруте
void printRouteInfo() {
  Serial.println("=== ИНФОРМАЦИЯ О МАРШРУТЕ ===");
  Serial.printf("Начальная точка: %.6f°, %.6f°\n", startLatitude, startLongitude);
  Serial.printf("Общая дистанция: %.0f м\n", TOTAL_DISTANCE);
  Serial.printf("Шаг обновления: %.0f м\n", DISTANCE_STEP);
  Serial.printf("Интервал обновления: %lu мс\n", UPDATE_INTERVAL);
  Serial.println("=============================\n");
}


Описание программы:

Основные функции:

moveAircraft() - перемещает самолет на заданное расстояние используя формулы сферической геометрии
checkAndUpdateCourse() - проверяет достижение конечных точек и изменяет курс на 180°
printCurrentPosition() - выводит текущие координаты и параметры полета
calculateDistance() - вычисляет расстояние между двумя точками по формуле гаверсинуса

Логика работы:

Самолет начинает движение с заданных координат под углом 70°
Каждую секунду происходит перемещение на 500 метров
При достижении 10 км курс меняется на 180° (250°)
При возврате к начальной точке курс снова меняется на 180° (70°)
Цикл повторяется бесконечно

Выходные данные:

Программа выводит в Serial Monitor:
Текущую пройденную дистанцию
Координаты (широта, долгота)
Текущий курс
Уведомления о смене курса

Настройки:

DISTANCE_STEP - шаг перемещения (500 м)
TOTAL_DISTANCE - общая дистанция (10000 м)
UPDATE_INTERVAL - интервал обновления (1000 мс)

Программа использует точные географические вычисления и учитывает кривизну Земли для корректного расчета координат.