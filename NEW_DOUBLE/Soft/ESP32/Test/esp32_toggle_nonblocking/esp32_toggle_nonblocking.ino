/*
  ESP32‑S3 — мгновенное включение/выключение прибора одной кнопкой.
  Не ждем отпускания кнопки: действие стартует сразу после 3 секунд удержания.
  Пины:
    7   — вход кнопки (LOW при нажатии, с внутренним pull‑up)
    17  — сигнал управления прибором (HIGH = ВКЛ, LOW = ОТКЛ)
    20  — импульсный выход (HIGH на 5с при включении, HIGH на 6с при выключении)
*/

const uint8_t PIN_BTN   = 7;
const uint8_t PIN_POWER = 17;
const uint8_t PIN_PULSE = 20;

const unsigned long HOLD_TIME_MS  = 3000;   // 3 сек
const unsigned long PULSE_ON_MS   = 5000;   // 5 сек (включение)
const unsigned long PULSE_OFF_MS  = 6000;   // 6 сек (выключение)

bool deviceOn = false;
bool btnPrevState = HIGH;
unsigned long btnPressStart = 0;
bool holdProcessed = false;

enum State {
  Idle,
  WaitOnPulse,
  WaitOffPulse
};
State state = Idle;

// Для импульса
bool pulseActive = false;
unsigned long pulseStart = 0;
unsigned long currentPulseDuration = 0;

void setup() {
  pinMode(PIN_BTN,    INPUT_PULLUP);
  pinMode(PIN_POWER,  OUTPUT);
  pinMode(PIN_PULSE,  OUTPUT);

  digitalWrite(PIN_POWER, LOW);
  digitalWrite(PIN_PULSE, LOW);
}

void loop() {
  unsigned long now = millis();
  bool btnState = digitalRead(PIN_BTN);

  switch (state) {
    case Idle:
      if (btnPrevState == HIGH && btnState == LOW) {
        // Кнопку только что нажали — фиксируем время
        btnPressStart = now;
        holdProcessed = false;
      }

      if (btnState == LOW) { // Кнопка удерживается
        if (!holdProcessed && (now - btnPressStart >= HOLD_TIME_MS)) {
          // Прошло 3 сек удержания — инициируем действие
          holdProcessed = true;
          if (!deviceOn) {
            // Включение: сразу прибор ON, потом импульс на 5 сек
            digitalWrite(PIN_POWER, HIGH);
            deviceOn = true;
            startPulse(PULSE_ON_MS);
            state = WaitOnPulse;
          } else {
            // Выключение: сначала импульс на 6 сек, после импульса — отключаем прибор
            startPulse(PULSE_OFF_MS);
            state = WaitOffPulse;
          }
        }
      }
      break;

    case WaitOnPulse:
      if (updatePulse(now)) {
        state = Idle;
      }
      break;
    case WaitOffPulse:
      if (updatePulse(now)) {
        digitalWrite(PIN_POWER, LOW);
        deviceOn = false;
        state = Idle;
      }
      break;
  }

  btnPrevState = btnState;
}

// Запуск импульса на PIN_PULSE
void startPulse(unsigned long duration) {
  digitalWrite(PIN_PULSE, HIGH);
  pulseActive = true;
  pulseStart = millis();
  currentPulseDuration = duration;
}

// Проверка окончания импульса; выключаем PIN_PULSE и возвращаем true, если завершён
bool updatePulse(unsigned long now) {
  if (pulseActive && (now - pulseStart >= currentPulseDuration)) {
    digitalWrite(PIN_PULSE, LOW);
    pulseActive = false;
    return true;
  }
  return false;
}

/*
Кратко по изменённой логике
Реакция на нажатие наступает именно спустя 3 секунды удержания (ещё до отпускания!).
Повторное выполнение в том же удержании предотвращается переменной holdProcessed.
Всё работает неблокирующе, через millis(), без delay().
После срабатывания действия (вкл/выкл) дальнейшее действие возможно только после отпускания и нового нажатия.
*/
