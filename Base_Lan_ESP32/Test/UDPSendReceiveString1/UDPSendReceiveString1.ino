/* --------------------------------------------------------------
   UDP‑Echo + Serial‑IP‑config (EEPROM)    ESP32‑S3 + Arduino IDE
   -------------------------------------------------------------- */

#include <SPI.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include <EEPROM.h>          // <‑‑ встроенная библиотека

/* ---------- Пины SPI для W5500 (можно переопределить) ---------- */
#define MISO_GPIO   13
#define MOSI_GPIO   11
#define SCK_GPIO    12
#define CS_GPIO    19          // CS‑pin
#define RST_GPIO   20          // Reset‑pin
#define INT_GPIO   16          // (не используется)

/* ---------- Прочие константы ---------- */
#define LED_PIN          4
#define EEPROM_SIZE     64      // всего 64 байта, 0‑3 – IP‑address
#define EEPROM_IP_ADDR  0       // стартовый адрес в EEPROM

/* ---------- MAC‑address (нужен любой уникальный) ---------- */
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

/* ---------- Параметры Ethernet ---------- */
IPAddress ip;                     // будет заполняться из EEPROM / default
unsigned int localPort = 10110;   // порт, на котором слушаем UDP

/* ---------- UDP buffers ---------- */
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
char ReplyBuffer[] = "ack nowledge";

EthernetUDP Udp;

/* ---------- Функции работы с EEPROM --------------------------- */
bool loadIPFromEEPROM(IPAddress &dst)
{
  uint8_t b[4];
  for (uint8_t i = 0; i < 4; ++i) b[i] = EEPROM.read(EEPROM_IP_ADDR + i); 
  // Если в EEPROM случайные данные (всё 0xFF) считаем, что адрес не записан
  if (b[0] == 0xFF && b[1] == 0xFF && b[2] == 0xFF && b[3] == 0xFF) 
  {
    return false;               // НЕ загружен
  }
  dst = IPAddress(b[0], b[1], b[2], b[3]);
  return true;
}

void saveIPToEEPROM(const IPAddress &src) {
  for (uint8_t i = 0; i < 4; ++i) EEPROM.write(EEPROM_IP_ADDR + i, src[i]);
  EEPROM.commit();               // ESP32 обязателен!
}

/* ---------- Вывод текущего IP в Serial ---------------------- */
void printCurrentIP() 
{
  Serial.print(F("Current IP = "));
  Serial.println(ip);
}

/* ---------- Утилита: разбор строки "a.b.c.d" ----------------- */
bool parseIPString(const String &s, IPAddress &out) 
{
  int octet[4];
  int cnt = sscanf(s.c_str(), "%d.%d.%d.%d",
                   &octet[0], &octet[1], &octet[2], &octet[3]);
  if (cnt != 4) return false;
  for (int i = 0; i < 4; ++i) {
    if (octet[i] < 0 || octet[i] > 255) return false;
    out[i] = (uint8_t)octet[i];
  }
  return true;
}

/* ---------- Командный парсер -------------------------------- */
void processSerialCommand(const String &cmd) 
{
  String s = cmd;
  s.trim();                     // убрать пробелы в начале/конце
  s.toLowerCase();

  if (s.startsWith("set ip")) 
  {
    int p = s.indexOf(' ');
    p = s.indexOf(' ', p + 1);   // позиция после "set ip"
    if (p == -1) {
      Serial.println(F("Usage: set ip a.b.c.d"));
      return;
    }
    String ipStr = s.substring(p + 1);
    IPAddress newIP;
    if (!parseIPString(ipStr, newIP)) 
    {
      Serial.println(F("Invalid IP format"));
      return;
    }
    // Сохраняем и перезапускаем Ethernet
    ip = newIP;
    saveIPToEEPROM(ip);
    Serial.print(F("Saved new IP: "));
    Serial.println(ip);
    // Перезапуск Ethernet (простое переинициализировать)
    Ethernet.begin(mac, ip);
    Udp.begin(localPort);
    Serial.println(F("Ethernet re‑initialized"));
  }
  else if (s == "show ip") 
  {
    printCurrentIP();
  }
  else if (s == "reset") 
  {
    // заполняем 0xFF → «не записано»
    for (uint8_t i = 0; i < 4; ++i) EEPROM.write(EEPROM_IP_ADDR + i, 0xFF);
    EEPROM.commit();
    Serial.println(F("EEPROM cleared – will use default IP on next reset"));
  }
  else if (s == "help") 
  {
    Serial.println(F("\nCommands:"));
    Serial.println(F("  set ip a.b.c.d   – store new IP"));
    Serial.println(F("  show ip          – print current IP"));
    Serial.println(F("  reset            – clear saved IP"));
    Serial.println(F("  help             – this list"));
  }
  else 
  {
    Serial.println(F("Unknown command. Type 'help' for list."));
  }
}

/* ---------- Setup ------------------------------------------------ */
void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }                 // дождаться открытия порта
  delay(200);
  Serial.println(F("\n=== UDP Echo + EEPROM IP config ==="));

  // ---------- инициализация EEPROM ----------
  EEPROM.begin(EEPROM_SIZE);            // 64 байта
  bool ipLoaded = loadIPFromEEPROM(ip);
  if (!ipLoaded) 
  {
    // заводской IP, если в EEPROM ничего нет
    ip = IPAddress(192, 168, 75, 247);
    Serial.println(F("No IP in EEPROM – using default"));
  } 
  else 
  {
    Serial.print(F("IP loaded from EEPROM: "));
    Serial.println(ip);
  }

  // ---------- инициализация пинов ----------
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(CS_GPIO, OUTPUT);
  digitalWrite(CS_GPIO, HIGH);
  pinMode(RST_GPIO, OUTPUT);
  digitalWrite(RST_GPIO, HIGH);
  pinMode(INT_GPIO, INPUT);            // (не используется)

  // ---------- Ethernet ----------
  Ethernet.begin(mac, ip);
  Udp.begin(localPort);
  Serial.print(F("UDP listening on port "));
  Serial.println(localPort);

  // ---------- таблица команд ----------
  Serial.println(F("\nEnter command (type 'help' for list):"));
}

/* ---------- Loop ------------------------------------------------- */
void loop() 
{
  // ----- проверяем Serial на наличие новых команд -----
  if (Serial.available()) 
  {
    String line = Serial.readStringUntil('\n');
    processSerialCommand(line);
  }

  // ----- UDP‑приём / ACK -----
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    Serial.print(F("Received packet of size "));
    Serial.println(packetSize);

    // Печатаем отправителя
    IPAddress remote = Udp.remoteIP();
    Serial.print(F("From "));
    Serial.print(remote);
    Serial.print(F(", port "));
    Serial.println(Udp.remotePort());

    // Читаем данные
    int len = Udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    packetBuffer[len] = '\0';               // делаем строку C‑style
    Serial.println(F("Contents:"));
    Serial.println(packetBuffer);

    // Отправляем ACK
    Udp.beginPacket(remote, Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket();
    Serial.println(F("ACK sent"));
  }

  delay(10);
}


//3. Как пользоваться
//Команда (в Serial‑мониторе)	Описание
//set ip 192.168.1.55	Сохранить новый IP в EEPROM и сразу переинициализировать Ethernet.
//show ip	Вывести текущий IP‑адрес (тот, который сейчас использует Ethernet).
//reset	Очистить запись в EEPROM (следующий старт вернётся к заводскому IP).
//help	Список всех команд.
//Важно: после set ip … Ethernet перезапускается без перезагрузки контроллера, поэтому любые текущие UDP‑соединения разрываются и открываются заново.
//
//4. Что делать, если нужно хранить другие параметры (маску, шлюз, порт)
//Расширьте EEPROM‑структуру. Пример (в начале файла):
//
//struct NetConfig {
//  uint8_t ip[4];
//  uint8_t gw[4];
//  uint8_t mask[4];
//  uint16_t udpPort;
//};
//Выделите в EEPROM блок sizeof(NetConfig) (например, EEPROM_SIZE = 32).
//Реализуйте loadConfig() / saveConfig() аналогично loadIPFromEEPROM().
