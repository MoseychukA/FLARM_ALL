//рабочий шаблон обмена по RS‑485 между двумя ESP32‑S3 (источник и приёмник) в стиле Arduino/Apduino IDE с использованием FreeRTOS (все задачи закреплены за ядром 0). Передаются только изменившиеся объекты, фреймы с CRC‑16, подтверждение ACK/NACK, сегментация “по объектам” (каждый объект — отдельный пакет). В источнике — генерация тестовых данных; в приёмнике — вывод в Serial. Управление направлением RS‑485 через DE/RE.
//
//Ключевые параметры:
//
//Интерфейсы и пины:
//Источник: Serial2 TX=GPIO18, RX=GPIO17, DE/RE=GPIO21
//Приёмник: Serial1 TX=GPIO39, RX=GPIO38, DE/RE=GPIO40
//Скорость: 921600 бод (можно менять через define)
//Ограничение по времени на пакет: при такой скорости и размере пакета <256 байт время передачи единичного фрейма < 3 мс
//Протокол:
//Преамбула: 0xAA 0x55
//ver (1), type (1), index (1), seq (1), length (2), payload (length), CRC16-IBM (2) — CRC считается по полям [ver..payload]
//Типы: 0x01 — Container[i], 0x02 — ThisAircraft, 0x03 — Analog, 0x04 — AuxFlags, 0x05 — Heartbeat, 0xF0 — ACK, 0xF1 — NACK
//ACK payload: orig_type (1), orig_index (1), orig_seq (1), status (1) — 0x06 ACK, 0x15 NACK
//Передаём “только изменённые данные”: отправляем целиком объект/структуру, но лишь при обнаружении изменений относительно последней отправленной копии
//Контроль приёма: проверка CRC, ответ ACK/NACK, повтор до 3 попыток
//Допполя источника: передаются отдельным фреймом AuxFlags + код аналогового сигнала uint16_t
//Схема обмена (мермаид):

//
//Код 2. Приёмник (ESP32S3, Serial1: TX39, RX38, DE/RE40)
//Скетч “receiver_rs485_esp32s3.ino”:

#include <Arduino.h>

// ================== Общие типы данных ==================
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
  float     course;        /* CoG */
  float     speed;         /* knots */
  uint8_t   aircraft_type;
  char      flight[16];    // Flight number
  int       vert_rate;     // Vertical rate
  int       Squawk;        // Squawk
  time_t    timemsg;       // Время передачи сообщения
  float     vs;            // fpm
  float     geoid_separation; // m
  uint16_t  hdop;          // cm
  int8_t    rssi;          // SX1276 only
  float     distance;
  float     bearing;
  uint8_t   signal_source;
  time_t    seen;          // время последнего приёма пакета
  uint8_t   hour_msg;
  uint8_t   min_msg;
  uint16_t  delay_time_msg;
  uint8_t   callsign[8];
} ufo_t;

ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;

typedef struct {
  bool     new_flag_M;
  uint8_t  new_buttton_M;
  bool     setMessageRead_M;
  bool     MessageRead_M;
  bool     SOS_Sprite_on_off_M;
  bool     SOS_View_on_off_M;
  bool     new_SOS_flag_M;
  bool     confirm_message_M;
  char     msg_resp_M[60];
  bool     isValidGNSS_M;
  uint8_t  FLYRF_MODE_TEST_M;
} aux_t;

aux_t AuxFlags;
uint16_t analog_code_M = 0;

// ================== RS485 / Протокол ==================
#define RS485_SERIAL         Serial1
#define RS485_TX_PIN         39
#define RS485_RX_PIN         38
#define RS485_DE_PIN         40

#define RS485_BAUD           921600
#define RS485_CONFIG         SERIAL_8N1

enum : uint8_t {
  PKT_CONTAINER = 0x01,
  PKT_THISAC    = 0x02,
  PKT_ANALOG    = 0x03,
  PKT_AUX       = 0x04,
  PKT_HEARTBEAT = 0x05,
  PKT_ACK       = 0xF0,
  PKT_NACK      = 0xF1
};

#pragma pack(push,1)
struct FrameHeader {
  uint16_t preamble; // 0xAA55
  uint8_t  ver;      // 0x01
  uint8_t  type;
  uint8_t  index;    // 0..7, 0xFF, 0
  uint8_t  seq;      // seq от источника
  uint16_t length;   // payload length
};
#pragma pack(pop)

#define PREAMBLE 0x55AAu
#define PROTO_VER 0x01
#define MAX_PAYLOAD 256

static uint16_t crc16_ibm(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else crc >>= 1;
    }
  }
  return crc;
}

static void rs485SetTX(bool enable) {
  digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
  if (enable) delayMicroseconds(50);
}

// ============== FreeRTOS объекты (ядро 0) ==============
SemaphoreHandle_t serialMutex;

// ============== Парсер и ACK ===========================
static bool parseOneFrame(Stream& s, FrameHeader& hdr, uint8_t* payloadBuf, uint16_t& crc) {
  static uint8_t state = 0;
  uint8_t b;
  while (s.available()) {
    b = s.read();
    if (state == 0 && b == 0xAA) { state = 1; continue; }
    if (state == 1 && b == 0x55) {
      size_t need = sizeof(FrameHeader) - 2;
      uint8_t* p = ((uint8_t*)&hdr) + 2;
      size_t got = 0;
      while (got < need) {
        int v = s.read();
        if (v < 0) { state = 2; goto WAIT_CONT; }
        p[got++] = (uint8_t)v;
      }
      state = 3;
WAIT_CONT:
      if (state != 3) return false;
      if (hdr.ver != PROTO_VER || hdr.length > MAX_PAYLOAD) {
        state = 0; continue;
      }
      for (uint16_t i = 0; i < hdr.length; ++i) {
        int v = -1; while ((v = s.read()) < 0) { delayMicroseconds(50); }
        payloadBuf[i] = (uint8_t)v;
      }
      uint8_t* c = (uint8_t*)&crc;
      for (int i = 0; i < 2; ++i) {
        int v = -1; while ((v = s.read()) < 0) { delayMicroseconds(50); }
        c[i] = (uint8_t)v;
      }
      state = 0;
      return true;
    } else {
      state = 0;
    }
  }
  return false;
}

static void sendAckLike(uint8_t type, uint8_t index, uint8_t seq, bool positive) {
  uint8_t payload[4];
  payload[0] = type;
  payload[1] = index;
  payload[2] = seq;
  payload[3] = positive ? 0x06 : 0x15;

  FrameHeader hdr;
  hdr.preamble = PREAMBLE;
  hdr.ver = PROTO_VER;
  hdr.type = positive ? PKT_ACK : PKT_NACK;
  hdr.index = 0;
  hdr.seq = 0;
  hdr.length = sizeof(payload);

  uint8_t header_and_payload[sizeof(FrameHeader)-2 + sizeof(payload)];
  memcpy(&header_and_payload[0], &hdr.ver, sizeof(FrameHeader)-2);
  memcpy(&header_and_payload[sizeof(FrameHeader)-2], payload, sizeof(payload));
  uint16_t crc = crc16_ibm(header_and_payload, sizeof(header_and_payload));

  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    rs485SetTX(true);
    RS485_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
    RS485_SERIAL.write(payload, sizeof(payload));
    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.flush();
    delayMicroseconds(200);
    rs485SetTX(false);
    xSemaphoreGive(serialMutex);
  }
}

// ============== Приёмная задача ========================
void RxTask(void* arg) {
  FrameHeader hdr;
  uint8_t payload[MAX_PAYLOAD];
  uint16_t rxCrc;

  for (;;) {
    if (!RS485_SERIAL.available()) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    if (!parseOneFrame(RS485_SERIAL, hdr, payload, rxCrc)) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    uint8_t buf[sizeof(FrameHeader)-2 + MAX_PAYLOAD];
    memcpy(buf, &hdr.ver, sizeof(FrameHeader)-2);
    memcpy(buf + sizeof(FrameHeader)-2, payload, hdr.length);
    uint16_t calc = crc16_ibm(buf, (sizeof(FrameHeader)-2) + hdr.length);

    bool ok = (calc == rxCrc);

    // ACK/NACK для служебных
    if (hdr.type == PKT_ACK || hdr.type == PKT_NACK) {
      // Приёмник служебные подтверждения от источника не использует
      continue;
    }

    if (!ok) {
      sendAckLike(hdr.type, hdr.index, hdr.seq, false);
      Serial.printf("[RX] CRC FAIL type=%02X idx=%u seq=%u\r\n", hdr.type, hdr.index, hdr.seq);
      continue;
    }

    // Обработка полезных данных
    switch (hdr.type) {
      case PKT_CONTAINER: {
        if (hdr.index < MAX_TRACKING_OBJECTS && hdr.length == sizeof(ufo_t)) {
          memcpy(&Container[hdr.index], payload, sizeof(ufo_t));
          Serial.printf("[RX] Container[%u]: lat=%.6f lon=%.6f alt=%.1f flight=%s\r\n",
            hdr.index,
            Container[hdr.index].latitude,
            Container[hdr.index].longitude,
            Container[hdr.index].altitude,
            Container[hdr.index].flight);
          sendAckLike(hdr.type, hdr.index, hdr.seq, true);
        } else {
          sendAckLike(hdr.type, hdr.index, hdr.seq, false);
        }
      } break;

      case PKT_THISAC: {
        if (hdr.length == sizeof(ufo_t)) {
          memcpy(&ThisAircraft, payload, sizeof(ufo_t));
          Serial.printf("[RX] ThisAircraft: lat=%.6f lon=%.6f alt=%.1f flight=%s\r\n",
            ThisAircraft.latitude,
            ThisAircraft.longitude,
            ThisAircraft.altitude,
            ThisAircraft.flight);
          sendAckLike(hdr.type, hdr.index, hdr.seq, true);
        } else {
          sendAckLike(hdr.type, hdr.index, hdr.seq, false);
        }
      } break;

      case PKT_ANALOG: {
        if (hdr.length == sizeof(uint16_t)) {
          memcpy(&analog_code_M, payload, sizeof(uint16_t));
          Serial.printf("[RX] Analog code: %u\r\n", (unsigned)analog_code_M);
          sendAckLike(hdr.type, hdr.index, hdr.seq, true);
        } else {
          sendAckLike(hdr.type, hdr.index, hdr.seq, false);
        }
      } break;

      case PKT_AUX: {
        if (hdr.length == sizeof(aux_t)) {
          memcpy(&AuxFlags, payload, sizeof(aux_t));
          Serial.printf("[RX] AUX: new_flag=%d btn=%u MSG='%s' GNSS=%d MODE=%u\r\n",
            (int)AuxFlags.new_flag_M,
            AuxFlags.new_buttton_M,
            AuxFlags.msg_resp_M,
            (int)AuxFlags.isValidGNSS_M,
            AuxFlags.FLYRF_MODE_TEST_M);
          sendAckLike(hdr.type, hdr.index, hdr.seq, true);
        } else {
          sendAckLike(hdr.type, hdr.index, hdr.seq, false);
        }
      } break;

      case PKT_HEARTBEAT: {
        // Можно ничего не делать, только ACK
        sendAckLike(hdr.type, hdr.index, hdr.seq, true);
      } break;

      default: {
        sendAckLike(hdr.type, hdr.index, hdr.seq, false);
      } break;
    }
  }
}

// ================== Arduino ============================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("RS485 Receiver start");

  pinMode(RS485_DE_PIN, OUTPUT);
  rs485SetTX(false);

  RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

  serialMutex = xSemaphoreCreateMutex();

  // Все задачи на ядре 0
  xTaskCreatePinnedToCore(RxTask, "RxTask", 8192, nullptr, 3, nullptr, 0);
}
 
void loop() {
  // Вся логика в задачах на core 0
}
//Примечания по эксплуатации:
//
//На обеих платах убедитесь, что DE и /RE RS‑485 трансивера объединены и заведены на указанный пин (GPIO21 на источнике, GPIO40 на приёмнике).
//Общая “земля” между платами обязательна.
//Скорость 921600 может потребовать качественных трансиверов/проводки. При проблемах снизьте до 460800 или 230400, изменив RS485_BAUD.
//“Только изменённые данные” реализовано по принципу “отправка объекта целиком при изменении”. При желании можно оптимизировать до диффов по полям и/или порогов для float.
//Ограничение 0.3 секунды на пакет выдерживается с большим запасом: при 921600 бод и размере до ~200 байт время передачи единичного кадра ~2–3 мс. Даже с повторами и ожиданием ACK в коде установлены таймауты в десятки миллисекунд.
//Если нужно, могу:
//
//Добавить журналирование статистики (потери, повторы, задержки).
//Ввести пороги изменений для float полей.
//Реализовать сборку нескольких объектов в один кадр с фрагментацией.