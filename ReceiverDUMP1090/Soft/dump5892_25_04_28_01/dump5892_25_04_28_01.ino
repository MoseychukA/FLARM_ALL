/*
 * dump5892.ino
 */

#include <stdlib.h>
#include "dump5892.h"
#include "EEPROMRF.h"

int bytes_per_ms = (SERIAL_OUT_BR / 12000);  // output rate - allows 12 serial bits per byte
static bool has_serial2 = false;

void setup()
{
  Serial.setTxBufferSize(OUTPUT_BUF_SIZE);  // needs to be done before begin()!
  Serial.begin(SERIAL_OUT_BR, SERIAL_8N1);
  delay(50);

  Serial.println();
  Serial.print(F(FIRMWARE_IDENT));
  Serial.print(F(" version: "));
  Serial.println(F(FIRMWARE_VERSION));
  Serial.println(F("Copyright (C) 2025 Decima"));
  Serial.println();

  Serial.print("** File: ");
  String ver_soft = __FILE__;
  int val_srt = ver_soft.lastIndexOf('\\');
  ver_soft.remove(0, val_srt + 1);
  val_srt = ver_soft.lastIndexOf('.');
  ver_soft.remove(val_srt);
  Serial.println(ver_soft);
  Serial.println();
  Serial.flush();

  EEPROM_setup();
  minrange10 = 10 * settings->minrange;
  maxrange10 = 10 * settings->maxrange;

  if (settings->outbaud) 
  {
    Serial.printf("switching output to %d baud rate\n", HIGHER_OUT_BR);
    delay(200);
    Serial.end();
    delay(500);
    Serial.setTxBufferSize(OUTPUT_BUF_SIZE);
    Serial.begin(HIGHER_OUT_BR, SERIAL_8N1);
    bytes_per_ms = (HIGHER_OUT_BR / 12000);
  }

  traffic_setup();

  CPRRelative_setup();
  if (reflat == 0 || reflon == 0)
      Serial.println("\n>>>> Reference lat/lon not set, positions will be wrong!\n"); //Не установлены координаты широты/долготы, позиции будут неверными!

  //if (settings->rx_pin > 16) // if (settings->rx_pin > 39) 
  //{
  //    Serial.println("\n>>>> Invalid RX GPIO pin, not starting Serial2\n");
  //}
  //else if (settings->tx_pin > 17) //(settings->tx_pin > 33)
  //{
  //    Serial.println("\n>>>> Invalid TX GPIO pin, not starting Serial2\n");
  //}
  //else 
  //{
      Serial2.setRxBufferSize(INPUT_BUF_SIZE);
      Serial2.begin(SERIAL_IN_BR, SERIAL_8N1, 16, 17);
      //Serial2.begin(SERIAL_IN_BR, SERIAL_8N1, settings->rx_pin, settings->tx_pin);
      has_serial2 = true;
  //}

  timenow = 100;
  delay(1000);
  pause5892();
  show_settings();
  reset5892();
}

char *time_string(bool withdate)
{
    static char s[20];    // at most: yyyy/mm/dd hh:mm:ss
    if (withdate && ourclock.year) {
        snprintf(s,20,"20%02d/%02d/%02d %02d:%02d:%02d",
             ourclock.year,ourclock.month,ourclock.day,
             ourclock.hour,ourclock.minute,ourclock.second);
    } else {
        snprintf(s,20,"%02dd%02dh%02dm%02ds",
             ourclock.day,ourclock.hour,ourclock.minute,ourclock.second);
    }
    return s;
}

static void in_discard()
{
    Serial.println("...");
    ++in_discards;
}

// this is not actually used:
static void out_discard()
{
    Serial.println(".");
    ++out_discards;
}

// оцените состояние буфера последовательного вывода по количеству отправленных байтов и времени,
// поскольку Serial.availableForWrite() не работает в ядре Arduino ESP32 v2.0.3
// (см. https://github.com/espressif/arduino-esp32/issues/6697)
static bool output_maybe(char *p, int n)
{
    static int output_bytes = 0;
    static uint32_t last_output_ms = 0;
    uint32_t now_ms = millis();
    int sent_since;                  // потенциально отправлено в течение временного интервала
    if (now_ms == last_output_ms)    // меньше миллисекунды
        sent_since = bytes_per_ms;   // не застревайте
    else
        sent_since = (now_ms - last_output_ms) * bytes_per_ms;
    if (output_bytes < sent_since || Serial.availableForWrite() == 128) 
    {
        // - в ESP32 Core 2.0.3 "128" означает пустой FIFO (и, следовательно, пустой буфер)
       // - в более поздних версиях ESP32 Core необходимо пересмотреть это
        output_bytes = 0;   // буфер очищен
    }
    else 
    {
        output_bytes -= sent_since;
    }
    last_output_ms = now_ms;
    if (output_bytes + n > (OUTPUT_BUF_SIZE-128)) 
    {
        // предположительно недостаточно места в буфере
        Serial.println(".");
        ++out_discards;
        return false;
    }
    Serial.write(p, n);
    output_bytes += n;
    return true;
}

void output_raw()
{
    buf[inputchars++] = ';';
    buf[inputchars++] = '\r';
    buf[inputchars++] = '\n';
    output_maybe(buf, inputchars);
    inputchars = 0;      // начать новое предложение ввода
}

/*
ALL
IDENTITY
POSITION
VELOCITY
GNSS
AIR
rssi
DF
ID
callsign
type
subtype
aircraft_type
lat
lon
alt_type
altitude
vert_rate
alt_diff
nsv
ewv
groundspeed
track
airspeed
heading
*/
void output_decoded()
{
    //if (mm.frame != 17 && mm.frame != 18)
    //    return;
    // создаем одну строку текста о последнем полученном сообщении
    const char *cs = ((fo.callsign[0] != '\0' || settings->format!=TXTFMT)? fo.callsign : "        ");
    const char *fmt;
    if (settings->dstbrg) 
    {
      if (settings->format==TABFMT)
        fmt = "%s\t%02d\t%02d\t%c\t%06X\t%s\t%d\t%.1f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\r\n";
      else if (settings->format==CSVFMT)
        fmt = "%s,%02d,%02d,%c,%06X,%s,%d,%.1f,%d,%d,%d,%d,%d,%d,%d,%d\r\n";
      else // TXTFMT
        fmt = "%s %02d %02d %c %06X %s %02d %5.1f %03d %5d %5d %5d %4d %4d %3d %3d\r\n";
            //tm rssi DF msgtyp ID cs actyp dst brg altitud altdif vs nsv ewv aspd hdg
      snprintf(parsed, PARSE_BUF_SIZE, fmt,
        time_string(true),
        fo.rssi, mm.frame, mm.msgtype, fo.addr, cs, fo.aircraft_type,
        fo.distance, fo.bearing,
        fo.altitude, fo.alt_diff, fo.vert_rate,
        fo.nsv, fo.ewv, fo.airspeed, fo.heading);
    }
    else 
    {
      if (settings->format==TABFMT)
        fmt = "%s\t%d\t%d\t%c\t%06X\t%s\t%d\t%.4f\t%.4f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\r\n";
      else if (settings->format==CSVFMT)
        fmt = "%s,%d,%d,%c,%06X,%s,%d,%.4f,%.4f,%d,%d,%d,%d,%d,%d,%d\r\n";
      else // TXTFMT
        fmt = "%s %02d %02d %c %06X %s %02d %9.4f %9.4f %5d %5d %5d %4d %4d %3d %3d\r\n";
             //tm rssi DF msgtyp ID cs actyp lat lon altitud altdif vs nsv ewv aspd hdg
      snprintf(parsed, PARSE_BUF_SIZE, fmt,
        time_string(true),
        fo.rssi, mm.frame, mm.msgtype, fo.addr, cs, fo.aircraft_type,
        fo.latitude, fo.longitude, fo.altitude, fo.alt_diff, fo.vert_rate,
        fo.nsv, fo.ewv, fo.airspeed, fo.heading);
    }
    parsedchars = strlen(parsed);
    output_maybe(parsed, parsedchars);
}

void output_page()
{
    // выводить страницу только в том случае, если есть новые данные от отслеживаемого самолета
    int i;
    if (settings->follow != 0) 
    {
        i = find_traffic_by_addr(settings->follow);
        if (i == 0)
            return;
        --i;      // индексация от базы 1 до базы 0
    }
    else 
    {
       // если выбран формат страницы и нет "отслеживаемых" самолетов
       // то показывать ближайший самолет или любой самолет
        i = find_closest_traffic();
        if (i > 0) 
        {
            --i;      // индексация от базы 1 до базы 0
        }
        else 
        {
            for (i=0; i < MAX_TRACKING_OBJECTS; i++) 
            {
               if (container[i].addr)
                   break;
            }
            if (i == MAX_TRACKING_OBJECTS)          // нет отслеживаемых самолетов
                return;
        }
    }
    //traffic_update(i);
    ufo_t *fop = &container[i];
    if (fop->reporttime >= fop->positiontime)        // ничего нового, чтобы сообщить
        return;
    if (timenow < fop->reporttime + 3)              // не сообщайте слишком часто
        return;
    fop->reporttime = timenow;
    Serial.println("\n----------------------------------------\n");
    // создать страницу текста о самолете, за которым следят
    uint32_t timesince = timenow - fop->positiontime;
    const char *cs = ((fo.callsign[0] != '\0')? fo.callsign : "        ");
    snprintf(parsed, PARSE_BUF_SIZE,
    "\%s      %d seconds since last position report    RSSI=%02d\n\
    ICAO ID: %06X   Callsign: %s    Aircraft Type: %s\n\
    Latitude = %9.4f   Longitude = %9.4f\n\
         - From here:  %5.1f nm,   %d bearing\n\
    Altitude = %5d (%s) (GNSS altitude rel to baro altitude: %d)\n\
    Vertical speed = %d fpm\n\
    Groundspeed = %4d knots   Track   = %3d\n\
    Airspeed    = %4d knots   Heading = %3d\n",
        time_string(true), timesince, fop->rssi,
        fop->addr, cs, ac_type_label[fop->aircraft_type],
        fop->latitude, fop->longitude,
        fop->distance, fop->bearing,
        fop->altitude, (fop->alt_type? "GNSS" : "barometric"),
        fop->alt_diff, fop->vert_rate,
        fop->groundspeed, fop->track,
        fop->airspeed, fop->heading);
    parsedchars = strlen(parsed);
    Serial.write(parsed, parsedchars);  // может блокироваться, но это происходит только каждые 3 секунды или около того
}

// список активных записей в таблице трафика (те, которые содержат последние данные о местоположении)
void output_list()
{
  static bool active = false;
  static int tick;
  static uint32_t nexttime = 0;
  if (! active) {
      if (millis() < nexttime)
          return;
      nexttime = millis() + 4000;
      if (num_tracked == 0)
          return;               // попробуйте еще раз через 4 секунды
      active = true;
      tick = 0;
      //Serial.println("\n----------------------------------------\n");
      //Serial.println("");
  }
  else 
  {
      // активен, сообщает об одном самолете за итерацию loop() (без ожидания millis())
      tick++;
      if (tick >= MAX_TRACKING_OBJECTS) 
      {
          active = false;
          return;
      }
  }
  ufo_t *fop = &container[tick];
  if (fop->addr == 0)
      return;
  if (timenow > fop->positiontime + 3)     // в последнее время ничего не слышно
      return;
  if (timenow < fop->reporttime + 2)       // недавно сообщалось
      return;
  fop->reporttime = timenow;
  // создаем одну строку текста о каждом отслеживаемом самолете
  const char *fmt;
  const char *t = time_string(true);      // немного позже фактического времени позиции
  const char *cs = ((fo.callsign[0] != '\0' || settings->format!=TXTFMT)? fo.callsign : "        ");
  const char *g = (fop->alt_type? "g" : settings->format==TXTFMT? " " : "");
  if (settings->dstbrg) {
    if (settings->format==TABFMT)
      fmt = "[%d]\t%s\t%d\t%06X\t%s\t%d\t%.1f\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\r\n";
    else if (settings->format==CSVFMT)
      fmt = "[%d],%s,%d,%06X,%s,%d,%.1f,%d,%s,%d,%d,%d,%d,%d,%d,%d\r\n";
    else // TXTFMT
      fmt = "[%2d] %s %02d %06X %s %02d %5.1f %03d %s %5d %5d %5d %3d %3d %3d %3d\r\n";
           //idx time rssi ID cs actyp dst brg altitude altdif vs gspd trk aspd hdg
    snprintf(parsed, PARSE_BUF_SIZE, fmt,
      tick, t, fop->rssi, fop->addr, cs, fop->aircraft_type,
      fop->distance, fop->bearing,
      g, fop->altitude, fop->alt_diff, fop->vert_rate,
      fop->groundspeed, fop->track,
      fop->airspeed, fop->heading);
  } else {
    if (settings->format==TABFMT)
      fmt = "[%d]\t%s\t%d\t%06X\t%s\t%d\t%.4f\t%.4f\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\r\n";
    else if (settings->format==CSVFMT)
      fmt = "[%d],%s,%d,%06X,%s,%d,%.4f,%.4f,%s,%d,%d,%d,%d,%d,%d,%d\r\n";
    else // TXTFMT
      fmt = "[%2d] %s %02d %06X %s %02d %9.4f %9.4f %s %5d %5d %5d %3d %3d %3d %3d\r\n";
            //idx time rssi ID cs actyp lat lon altitud altdif vs gspd trk aspd hdg
    snprintf(parsed, PARSE_BUF_SIZE, fmt,
      tick, t, fop->rssi, fop->addr, cs, fop->aircraft_type,
      fop->latitude, fop->longitude,
      g, fop->altitude, fop->alt_diff, fop->vert_rate,
      fop->groundspeed, fop->track,
      fop->airspeed, fop->heading);
  }
  parsedchars = strlen(parsed);
  if (output_maybe(parsed, parsedchars) == false) {
      // попробуйте то же самое снова в следующий раз в цикле():
      fop->reporttime -= 2;
      --tick;
  }
}

static bool input_complete;

void input_loop()
{
    if (input_complete)
        inputchars = 0;          // начать новое предложение ввода
    input_complete = false;
    if (has_serial2 == false)
        return;
    int n = inputchars;
    if (n == 0)
    {                // ждем начала нового предложения
        if (Serial2.available() > (INPUT_BUF_SIZE - 256)) 
        {
            // входной буфер заполняется, очистите его
            Serial2.readBytes(buf, 256);  // отбрасываем некоторые входные данные
            in_discard();
        }
    }
    while (Serial2.available()) 
    {  // цикл до полного предложения или до отсутствия данных
        char c = Serial2.read();
        if (c=='*' || c=='+' || c=='#')
        {
            buf[0] = c;           // начать новое предложение, удалить все предыдущие данные
            n = 1;
        }
        else if (n == 0) 
        {      // ждем допустимого начального символа
            continue;
        }
        else if (c==';' || c=='\r' || c=='\n') 
        {   // законченное предложение
            if (n > 14 /* && n <= 32 */ ) 
            {
                input_complete = true;
                break;
            }
            n = 0;                 // недействительно, начать заново
        }
        else 
        {
            buf[n++] = c;
        }
    }
    inputchars = n;
}

void parse_loop()
{
    if (!input_complete) 
    {
        parsing_success = false;
        return;
    }

    if (inputchars > 0 && settings->parsed != RAWFMT) 
    {
        if (buf[0] == '*' || buf[0] == '+') 
        {        // ADS-B data received
            parsing_success = parse(buf, inputchars);
        }
        else if (buf[0] == '#') 
        {                                            // ответ на команды
            Serial.write(buf, inputchars);           // копировать в консоль
            Serial.println("");
        }
    }
}

void output_loop()
{
    if (settings->parsed == NOTHING)
        return;
    // форматы вывода на основе таблицы трафика:
    if (settings->parsed == LSTFMT) 
    {
        output_list();
        return;
    }
    if (settings->parsed == PAGEFMT)
    {
        if (settings->follow != 0 || num_tracked == 1 || find_closest_traffic() > 0)
            output_page();
        else
            output_list();
        return;
    }
    // форматы вывода на основе самого последнего сообщения:
    if (!input_complete)
        return;
    if (inputchars == 0)
        return;
    if (settings->parsed == RAWFMT) 
    {
        output_raw();
        return;
    }
    // форматы вывода на основе последнего проанализированного сообщения:
    if (parsing_success == false)
        return;
    parsing_success = false;
    if (settings->parsed == RAWFILT) {   // то же, что и RAW, но отфильтровано
        int i = find_traffic_by_addr(fo.addr);
        if (i == 0)                              // нет в таблице трафика
            return;                              // еще не получили сообщение о местоположении
        if (container[i-1].positiontime == 0)    // нет идентификационного сообщения или оно отфильтровано
            return;
        output_raw();
        return;
    }
    inputchars = 0;
    if (settings->parsed == FLDFMT) 
    {
        output_maybe(parsed, parsedchars);
        return;
    }
    output_decoded();
}

void cmd_loop()
{
    bool complete = false;
    int end_of_cmd = 0;
    while (Serial.available()) 
    {     // цикл до тех пор, пока не закончатся данные
        char c = Serial.read();
        if (c==';' || c=='\r' || c=='\n') {   // законченное предложение
            if (! complete) 
            {
                end_of_cmd = cmdchars;        // исключает ';'
                complete = true;
                // любые дополнительные символы будут приняты, но проигнорированы
            }
        }
        if (cmdchars >= 120) 
        {
            cmdchars = 0;
            complete = false;
        }
        cmdbuf[cmdchars++] = c;
    }
    if (complete) 
    {
        cmdbuf[end_of_cmd] = '\0';
        if (cmdbuf[0] == '*' || cmdbuf[0] == '+') 
        {   // смоделированные данные ADS-B
            if (end_of_cmd > 3) 
            {
                strcpy(buf, cmdbuf);
                inputchars = end_of_cmd;
                input_complete = true;
                parse_loop();
                traffic_loop();
                output_loop();
            }
        }
        else 
        {
            interpret_cmd(cmdbuf, end_of_cmd);
        }
        cmdchars = 0;
    }
}

void clock_loop()
{
    uint32_t ms = millis();
    if (ms >= ourclock.nextsecond) {
        if (ourclock.nextsecond == 0)
            ourclock.nextsecond = ms;
        ourclock.nextsecond += 1000;
        ++timenow;
        ++ourclock.second;
        if (ourclock.second == 60) {
            ourclock.second = 0;
            ++ourclock.minute;
            if (ourclock.minute == 60) {
                ourclock.minute = 0;
                ++ourclock.hour;
                if (ourclock.hour == 24) {
                    ourclock.hour = 0;
                    ++ourclock.day;
                    // игнорировать то, что происходит в конце месяца
                }
            }
        }
    }
}

void loop()
{
  input_loop();
  yield();
  parse_loop();
  yield();
  traffic_loop();
  yield();
  output_loop();
  yield();
  cmd_loop();
  clock_loop();
  yield();
}
