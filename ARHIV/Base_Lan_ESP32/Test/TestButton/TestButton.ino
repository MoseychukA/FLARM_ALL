/* --------------------------------------------------------------
 * ESP32‑S3 Arduino IDE – надёжный детектор:
 *   • одиночный клик
 *   • двойной клик
 *   • длительное удержание (Long press)
 * -------------------------------------------------------------- */

#include <Arduino.h>

// ----------------- Параметры -----------------
const int   BUTTON_PIN            = 48;   // Пин кнопки (подтягивается к VCC через pull‑up)
const uint32_t DEBOUNCE_MS        = 20;   // анти‑дребезг (мс)
const uint32_t LONG_PRESS_MS      = 800;  // порог длительного удержания (мс)
const uint32_t DOUBLE_CLICK_MAX_MS = 400; // максимум между двумя кликами (мс)

// ----------------- Коллбэки -----------------
void onSingleClick()   { Serial.println(F("Single click")); }
void onDoubleClick()   { Serial.println(F("Double click")); }
void onLongPress()     { Serial.println(F("Long press")); }

// ----------------- Переменные задачи -----------------
TaskHandle_t Task3 = nullptr;

// ----------------- Состояния FSM -----------------
enum BtnState { IDLE, PRESSED, RELEASED };

void ButtonTask(void *pvParameters) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // Кнопка замыкает в GND

  BtnState   state = IDLE;
  uint32_t   lastChangeTime = 0;          // время последней стабилизации уровня
  uint32_t   pressStartTime = 0;          // когда кнопка была нажата
  uint32_t   firstReleaseTime = 0;        // время первого отпускания (для двойного)
  uint8_t    clickCount = 0;              // сколько раз уже отпустили
  bool       longPressDetected = false;  // был ли уже зафиксирован Long press

  for (;;) {
    bool rawPressed = (digitalRead(BUTTON_PIN) == LOW); // LOW = нажата
    uint32_t now = millis();

    /* ------------------- Дебаунс ------------------- */
    if (rawPressed != (state == PRESSED)) {          // уровень изменился
      if (now - lastChangeTime >= DEBOUNCE_MS) {
        lastChangeTime = now;                        // запомнили момент стабилизации

        if (rawPressed) {                            // ---------- Нажата ----------
          state = PRESSED;
          pressStartTime = now;
          longPressDetected = false;                 // сбрасываем флаг при новом нажатии
        } else {                                     // ---------- Отпущена ----------
          state = RELEASED;
          // Если уже зафиксирован long press, игнорируем клик полностью
          if (longPressDetected) {
            // Сразу переходим в IDLE – клик не считается
            clickCount = 0;
            longPressDetected = false;
            state = IDLE;
          } else {
            // Обычный клик – учитываем
            clickCount++;
            firstReleaseTime = now;
          }
        }
      }
    }

    /* ------------------- Обработка состояний ------------------- */
    switch (state) {

      case PRESSED: {
        // Проверяем длительное удержание
        if (now - pressStartTime >= LONG_PRESS_MS && !longPressDetected) {
          onLongPress();                     // сразу оповещаем о длительном удержании
          longPressDetected = true;          // помечаем, что событие уже сгенерировано
        }
        break;
      }

      case RELEASED: {
        // Если уже был long press – ничего не делаем (см. выше)
        if (longPressDetected) {
          // Уже сбросились в IDLE, так что сюда не попадаем
          break;
        }

        // Ожидаем возможность второго клика
        if (clickCount == 1) {
          // Если прошёл таймаут без второго клика → одиночный клик
          if (now - firstReleaseTime > DOUBLE_CLICK_MAX_MS) {
            onSingleClick();
            clickCount = 0;
            state = IDLE;
          }
          // иначе остаёмся в RELEASED, ждём второй клик
        } else if (clickCount == 2) {
          // Два клика за короткое время → двойной
          onDoubleClick();
          clickCount = 0;
          state = IDLE;
        }
        break;
      }

      case IDLE:
      default:
        // Ничего не делаем
        break;
    }

    // Немного «отдыхаем», чтобы не загружать процессор
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  // На практике сюда никогда не попадаем
  vTaskDelete(nullptr);
}

/* -------------------- setup / loop -------------------- */
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }                       // ждём открытия порта
  Serial.println(F("\n--- ESP32‑S3 Button demo ---"));

  // Создаём задачу, привязанную к ядру 0
  xTaskCreatePinnedToCore(
    ButtonTask,        // функция задачи
    "BtnTask",         // имя задачи (для отладки)
    4096,              // стек (байт)
    nullptr,           // параметр задачи
    1,                 // приоритет
    &Task3,            // дескриптор
    0                  // ядро (0 или 1)
  );
}

void loop() {
  // Основной код программы может идти здесь.
  // В примере ничего не делаем – всё в ButtonTask.
}
