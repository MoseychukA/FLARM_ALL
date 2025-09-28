


 // ---------- ПИНЫ ----------
constexpr uint8_t PIN_NSS  = 46;   // CS
constexpr uint8_t PIN_RST  = 7;    // Reset
constexpr uint8_t PIN_IRQ  = 18;    // DIO0 (IRQ)
constexpr uint8_t PIN_DIO1 = 15;   // DIO1 – нужен BasicMAC

// ---------- LoRaWAN параметры (OTAA) ----------
static const uint8_t DEV_EUI[8] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77 };
static const uint8_t APP_EUI[8] = { 0x70,0xB3,0xD5,0x7E,0xD0,0x00,0x00,0x00 };
static const uint8_t APP_KEY[16] = { /* 16‑байтовый AppKey */ };

// ---------- Тестовые параметры ----------
uint8_t dr = 5;               // DR5 → SF7, BW125kHz
uint8_t sf = 7;               // Spreading Factor 7‑12
uint32_t bw = 125E3;          // 125, 250, 500 kHz
uint8_t cr = 1;               // 0‑3 → 4/5 … 4/8
int8_t txPower = 14;          // dBm

enum class Step { DR, SF, BW, CR, POWER, DONE };
Step curStep = Step::DR;

// ---------- Периодичность ----------
const uint32_t TX_INTERVAL_MS = 8000;
uint32_t lastTx = 0;
uint32_t pktCnt = 0;

#define LORAWAN_PREAMBLE_LENGTH 8

// ---------- Функции изменения параметров ----------
void applyParameters() 
{
    // Data Rate (DR) – основной способ в lora
    lora.setDR(dr);                // 0‑5 (EU868)

    // Если требуется задать SF/ BW/ CR вручную, делаем это через регистры:
    if (curStep == Step::SF) {
        lora.setSF(sf);            // 7‑12
    }
    if (curStep == Step::BW) {
        // lora позволяет менять полосу через setBandWidth()
        lora.setBandWidth(bw);
    }
    if (curStep == Step::CR) {
        // Coding Rate: 1 → 4/5, 2 → 4/6, 3 → 4/7, 4 → 4/8
        lora.setCodingRate(cr + 1);
    }
    if (curStep == Step::POWER) {
        lora.setTxPower(txPower);
    }
}

// ---------- Переход к следующему набору параметров ----------
void nextStep() {
    static uint8_t repeat = 0; // отправляем 5 пакетов в каждом режиме
    repeat++;
    if (repeat >= 5) {
        repeat = 0;
        // переходим к следующему этапу
        curStep = static_cast<Step>(static_cast<int>(curStep) + 1);
        if (curStep == Step::DONE) 
        {
            Serial.println(F("=== Тест завершён ==="));
        }
        else 
        {
            Serial.println(F("=== Переходим к следующему набору параметров ==="));
        }
    }
}

// ---------- Setup ----------
void setup() {
    Serial.begin(115200);
    while (!Serial) ;
    Serial.println(F("=== LoRa lora 868 MHz (ESP32‑S3) ==="));

    // Инициализация SPI‑линий
    lora.begin(868E6, PIN_NSS, PIN_RST, PIN_IRQ, PIN_DIO1);
    lora.setDevEUI(DEV_EUI);
    lora.setAppEUI(APP_EUI);
    lora.setAppKey(APP_KEY);

    // Присоединяемся к сети (OTAA)
    while (!lora.joinOTAA()) {
        Serial.println(F("Attempting OTAA join..."));
        delay(5000);
    }
    Serial.println(F("Joined LoRaWAN network!"));

    // Убираем Adaptive Data Rate, иначе он будет менять DR сам
    lora.setADR(false);

    // Применяем начальные параметры
    applyParameters();
    lastTx = millis();
}

// ---------- Loop ----------
void loop() {
    // Периодически отправляем пакет
    if (millis() - lastTx >= TX_INTERVAL_MS && lora.isJoined()) {
        lastTx = millis();

        // Формируем payload
        uint8_t payload[4];
        payload[0] = (pktCnt >> 24) & 0xFF;
        payload[1] = (pktCnt >> 16) & 0xFF;
        payload[2] = (pktCnt >> 8) & 0xFF;
        payload[3] = pktCnt & 0xFF;

        // Отправляем (не‑confirmable)
        bool ok = lora.sendPacket(payload, sizeof(payload));
        Serial.printf("[Tx] pkt=%lu  DR=%d  SF=%d  BW=%lukHz  CR=4/%d  TX=%ddBm  status=%s",
                      pktCnt, lora.getDR(), lora.getSF(),
                      lora.getBandWidth()/1000, lora.getCodingRate(),
                      lora.getTxPower(), ok ? "OK" : "FAIL");
        pktCnt++;

        // Меняем параметры только после каждой отправки
        if (curStep != Step::DONE) {
            // меняем параметр в зависимости от текущего шага
            switch (curStep) {
                case Step::DR:
                    dr = (dr == 5) ? 0 : dr + 1;   // 0‑5
                    lora.setDR(dr);
                    break;
                case Step::SF:
                    sf = (sf == 12) ? 7 : sf + 1;
                    lora.setSF(sf);
                    break;
                case Step::BW:
                    if (bw == 125E3) bw = 250E3;
                    else if (bw == 250E3) bw = 500E3;
                    else bw = 125E3;
                    lora.setBandWidth(bw);
                    break;
                case Step::CR:
                    cr = (cr == 3) ? 0 : cr + 1;
                    lora.setCodingRate(cr + 1);
                    break;
                case Step::POWER:
                    txPower = (txPower >= 20) ? 2 : txPower + 2;
                    lora.setTxPower(txPower);
                    break;
                default: break;
            }
            nextStep(); // считаем, сколько пакетов уже отправлено в текущем режиме
        }
    }

    // Обрабатываем внутренние задачи lora (RX‑пакеты, таймеры и т.д.)
    lora.handle();
}

/*
Как пользоваться
Скомпилируйте и загрузите LoRa_BasicMAC_868_S3.ino в ESP32‑S3.
Откройте Serial Monitor (115200 baud). После успешного OTAA‑join вы увидите сообщения вида:
[Tx] pkt=0  DR=5  SF=7  BW=125kHz  CR=4/5  TX=14dBm  status=OK
После каждых пяти пакетов скетч меняет один из параметров (DR → SF → BW → CR → TX‑Power).
По окончании всех комбинаций будет выведено === Тест завершён ===.
*/