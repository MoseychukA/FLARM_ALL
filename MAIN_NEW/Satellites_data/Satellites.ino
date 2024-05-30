// ПРИМЕР ВЫВОДИТ ДАННЫЕ NMEA ПОЛУЧАЕМЫЕ ПО UART: //  дата, время, координаты, скорость, направление, данные о спутниках.
 
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "boards.h"



// Defined using AXP2102
#define XPOWERS_CHIP_AXP2102

#include <Wire.h>
#include "XPowersLib.h"

bool  pmu_flag = 0;
XPowersPMU PMU;

const uint8_t i2c_sda = 21;
const uint8_t i2c_scl = 22;
const uint8_t pmu_irq_pin = 35;

void setFlag(void)
{
    pmu_flag = true;
}

//
#include <iarduino_GPS_NMEA.h>                    //  Подключаем библиотеку для расшифровки строк протокола NMEA получаемых по UART.
iarduino_GPS_NMEA gps;                            //  Объявляем объект gps для работы с функциями и методами библиотеки iarduino_GPS_NMEA.
                                                  //
uint8_t i[20][7];                                 //  Объявляем массив для получения данных о 20 спутниках в формате: {ID спутника (1...255), Отношение сигнал/шум (SNR) в дБ, тип навигационной системы (1-GPS/2-Глонасс/3-Galileo/4-Beidou/5-QZSS), Флаг участия спутника в позиционировании (1/0), Угол возвышения спутника (0°-горизонт ... 90°-зенит), Азимут положения спутника (0°...255°), Остаток азимута до 360° }.
char* sa[]={"NoName ","GPS    ","Глонасс","Galileo","Beidou ","QZSS   "}; // Определяем массив строк содержащих названия навигационных систем спутников.
char* wd[]={"Вс","Пн","Вт","Ср","Чт","Пт","Сб"};  //  Определяем массив строк содержащих по две первых буквы из названий дня недели.
                                                  //
#define SerialGNSS Serial1
static const uint32_t GPSBaud = 9600;



void setup(){                                     //
    // Serial.begin(9600);                          //  Инициируем работу с аппаратной шиной UART для вывода данных в монитор последовательного порта на скорости 9600 бит/сек.
     Serial.begin(115200);

     String ver_soft = __FILE__;
     int val_srt = ver_soft.lastIndexOf('\\');
     ver_soft.remove(0, val_srt + 1);
     val_srt = ver_soft.lastIndexOf('.');
     ver_soft.remove(val_srt);
     Serial.println(ver_soft);


     initBoard();
     // When the power is turned on, a delay is required.

     PoverPMU();

     delay(1500);

     SerialGNSS.begin(GPSBaud);


     //Serial1.begin(9600);                         //  Инициируем работу с аппаратной шиной UART для получения данных от GPS модуля на скорости 9600 бит/сек.
     //gps.begin(Serial1);                          //  Инициируем расшифровку строк NMEA указав объект используемой шины UART (вместо аппаратной шины, можно указывать программную).
     //gps.timeZone(3);                             //  Указываем часовой пояс (±12 часов), или GPS_AutoDetectZone для автоматического определения часового пояса по долготе.

}                                                 //
                                                  //
void loop()
{                                      //

    if (SerialGNSS.available() > 0)
    {
        char c = SerialGNSS.read();
        Serial.print(c);


    }


    // gps.read(i);                                 //  Читаем данные с получением информации о спутниках в массив i (чтение может занимать больше 1 секунды). Если указать второй параметр gps.read(i,true); то в массиве окажутся данные только тех спутников, которые принимают участие в позиционировании.
   //  Serial.print(i);
                                                  
                                                  
                                                  
                                                  // /* Время:                                    */  Serial.print(gps.Hours); Serial.print(":"); Serial.print(gps.minutes); Serial.print(":"); Serial.print(gps.seconds); Serial.print(" ");
    // /* Дата:                                     */  Serial.print(gps.day  ); Serial.print("."); Serial.print(gps.month  ); Serial.print("."); Serial.print(gps.year   ); Serial.print("г (");
    // /* Дополнительные данные даты:               */  Serial.print(wd[gps.weekday]); Serial.print("), UnixTime: "); Serial.print(gps.Unix); Serial.print(". ");
    // /* Координаты (широта, долгота, высота):     */  Serial.print("Ш: "); Serial.print(gps.latitude,5); Serial.print("°, Д: "); Serial.print(gps.longitude,5); Serial.print("°, В: "); Serial.print(gps.altitude,1); Serial.print("м. ");
    // /* Движение (скорость, курс):                */  Serial.print("Скорость: "); Serial.print(gps.speed); Serial.print("км/ч, "); Serial.print(gps.course); Serial.print("°. ");
    // /* Спутники (активные/наблюдаемые):          */  Serial.print("Спутники: "); Serial.print(gps.satellites[GPS_ACTIVE]); Serial.print("/"); Serial.print(gps.satellites[GPS_VISIBLE]); Serial.print(". ");
    // /* Геометрический фактор ухудшения точности: */  Serial.print("PDOP: "); Serial.print(gps.PDOP); Serial.print(", HDOP: "); Serial.print(gps.HDOP); Serial.print(", VDOP: "); Serial.print(gps.VDOP); Serial.print(". ");
    // /* Ошибка определения времени:               */  if(gps.errTim){Serial.print("Вемя недостоверно. "           );}
    // /* Ошибка определения даты:                  */  if(gps.errDat){Serial.print("Дата недостоверна. "           );}
    // /* Ошибка позиционирования:                  */  if(gps.errPos){Serial.print("Координаты недостоверны. "     );}
    // /* Ошибка определения скорости и курса       */  if(gps.errCrs){Serial.print("Скорость и курс недостоверны. ");}
    // /*                                           */  Serial.print("\r\n");
    //                                              //
    // for(uint8_t j=0; j<20; j++){                 //  Проходим по всем элементам массива «i».
    //    if( i[j][0] ){                            //  Если у спутника есть ID, то выводим информацию о нём:
    //        if(j<9){Serial.print(" ");}   Serial.print(j+1);         Serial.print(") "  );
    //        Serial.print("Спутник "    ); Serial.print(sa[i[j][2]]); Serial.print(" "   );
    //        Serial.print("ID: "        ); Serial.print(   i[j][0] ); Serial.print(". "  );
    //        Serial.print("Уровень: "   ); Serial.print(   i[j][1] ); Serial.print("дБ. ");
    //        Serial.print("Возвышение: "); Serial.print(   i[j][4] ); Serial.print("°. " );
    //        Serial.print("Азимут: "    ); Serial.print(   i[j][5]+i[j][6] ); Serial.print("°. " );
    //        if(i[j][3]){Serial.print("Участвует в позиционировании.");}
    //        Serial.print("\r\n");                 //
    //    }                                         //
    //}                                             //
}                                                 //



void PoverPMU()
{
    bool result = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, i2c_sda, i2c_scl);

    if (result == false) {
        Serial.println("PMU is not online..."); while (1)delay(50);
    }

    Serial.printf("getID:0x%x\n", PMU.getChipID());

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);


    // Get the VSYS shutdown voltage
    uint16_t vol = PMU.getSysPowerDownVoltage();
    Serial.printf("->  getSysPowerDownVoltage:%u\n", vol);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    PMU.setSysPowerDownVoltage(2600);

    vol = PMU.getSysPowerDownVoltage();
    Serial.printf("->  getSysPowerDownVoltage:%u\n", vol);


    // DC1 IMAX=2A
    // 1500~3400mV,100mV/step,20steps
    PMU.setDC1Voltage(3300);
    Serial.printf("DC1  : %s   Voltage:%u mV \n", PMU.isEnableDC1() ? "+" : "-", PMU.getDC1Voltage());

    // DC2 IMAX=2A
    // 500~1200mV  10mV/step,71steps
    // 1220~1540mV 20mV/step,17steps
    PMU.setDC2Voltage(1000);
    Serial.printf("DC2  : %s   Voltage:%u mV \n", PMU.isEnableDC2() ? "+" : "-", PMU.getDC2Voltage());

    // DC3 IMAX = 2A
    // 500~1200mV,10mV/step,71steps
    // 1220~1540mV,20mV/step,17steps
    // 1600~3400mV,100mV/step,19steps
    PMU.setDC3Voltage(3300);
    Serial.printf("DC3  : %s   Voltage:%u mV \n", PMU.isEnableDC3() ? "+" : "-", PMU.getDC3Voltage());

    // DCDC4 IMAX=1.5A
    // 500~1200mV,10mV/step,71steps
    // 1220~1840mV,20mV/step,32steps
    PMU.setDC4Voltage(1000);
    Serial.printf("DC4  : %s   Voltage:%u mV \n", PMU.isEnableDC4() ? "+" : "-", PMU.getDC4Voltage());

    // DC5 IMAX=2A
    // 1200mV
    // 1400~3700mV,100mV/step,24steps
    PMU.setDC5Voltage(3300);
    Serial.printf("DC5  : %s   Voltage:%u mV \n", PMU.isEnableDC5() ? "+" : "-", PMU.getDC5Voltage());

    //ALDO1 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO1Voltage(3300);

    //ALDO2 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO2Voltage(3300);

    //ALDO3 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO3Voltage(3300);

    //ALDO4 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO4Voltage(3300);

    //BLDO1 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setBLDO1Voltage(3300);

    //BLDO2 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setBLDO2Voltage(3300);

    //CPUSLDO IMAX=30mA
    //500~1400mV,50mV/step,19steps
    PMU.setCPUSLDOVoltage(1000);

    //DLDO1 IMAX=300mA
    //500~3400mV, 100mV/step,29steps
    PMU.setDLDO1Voltage(3300);

    //DLDO2 IMAX=300mA
    //500~1400mV, 50mV/step,2steps
    PMU.setDLDO2Voltage(3300);


    PMU.enableDC1();
    PMU.enableDC2();
    PMU.enableDC3();
    PMU.enableDC4();
    PMU.enableDC5();
    PMU.enableALDO1();
    PMU.enableALDO2();
    PMU.enableALDO3();
    PMU.enableALDO4();
    PMU.enableBLDO1();
    PMU.enableBLDO2();
    PMU.enableCPUSLDO();
    PMU.enableDLDO1();
    PMU.enableDLDO2();


    Serial.println("DCDC=======================================================================");
    Serial.printf("DC1  : %s   Voltage:%u mV \n", PMU.isEnableDC1() ? "+" : "-", PMU.getDC1Voltage());
    Serial.printf("DC2  : %s   Voltage:%u mV \n", PMU.isEnableDC2() ? "+" : "-", PMU.getDC2Voltage());
    Serial.printf("DC3  : %s   Voltage:%u mV \n", PMU.isEnableDC3() ? "+" : "-", PMU.getDC3Voltage());
    Serial.printf("DC4  : %s   Voltage:%u mV \n", PMU.isEnableDC4() ? "+" : "-", PMU.getDC4Voltage());
    Serial.printf("DC5  : %s   Voltage:%u mV \n", PMU.isEnableDC5() ? "+" : "-", PMU.getDC5Voltage());
    Serial.println("ALDO=======================================================================");
    Serial.printf("ALDO1: %s   Voltage:%u mV\n", PMU.isEnableALDO1() ? "+" : "-", PMU.getALDO1Voltage());
    Serial.printf("ALDO2: %s   Voltage:%u mV\n", PMU.isEnableALDO2() ? "+" : "-", PMU.getALDO2Voltage());
    Serial.printf("ALDO3: %s   Voltage:%u mV\n", PMU.isEnableALDO3() ? "+" : "-", PMU.getALDO3Voltage());
    Serial.printf("ALDO4: %s   Voltage:%u mV\n", PMU.isEnableALDO4() ? "+" : "-", PMU.getALDO4Voltage());
    Serial.println("BLDO=======================================================================");
    Serial.printf("BLDO1: %s   Voltage:%u mV\n", PMU.isEnableBLDO1() ? "+" : "-", PMU.getBLDO1Voltage());
    Serial.printf("BLDO2: %s   Voltage:%u mV\n", PMU.isEnableBLDO2() ? "+" : "-", PMU.getBLDO2Voltage());
    Serial.println("CPUSLDO====================================================================");
    Serial.printf("CPUSLDO: %s Voltage:%u mV\n", PMU.isEnableCPUSLDO() ? "+" : "-", PMU.getCPUSLDOVoltage());
    Serial.println("DLDO=======================================================================");
    Serial.printf("DLDO1: %s   Voltage:%u mV\n", PMU.isEnableDLDO1() ? "+" : "-", PMU.getDLDO1Voltage());
    Serial.printf("DLDO2: %s   Voltage:%u mV\n", PMU.isEnableDLDO2() ? "+" : "-", PMU.getDLDO2Voltage());
    Serial.println("===========================================================================");

    // Set the time of pressing the button to turn off
    PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    uint8_t opt = PMU.getPowerKeyPressOffTime();
    Serial.print("PowerKeyPressOffTime:");
    switch (opt) {
    case XPOWERS_POWEROFF_4S: Serial.println("4 Second");
        break;
    case XPOWERS_POWEROFF_6S: Serial.println("6 Second");
        break;
    case XPOWERS_POWEROFF_8S: Serial.println("8 Second");
        break;
    case XPOWERS_POWEROFF_10S: Serial.println("10 Second");
        break;
    default:
        break;
    }
    // Set the button power-on press time
    PMU.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
    opt = PMU.getPowerKeyPressOnTime();
    Serial.print("PowerKeyPressOnTime:");
    switch (opt) {
    case XPOWERS_POWERON_128MS: Serial.println("128 Ms");
        break;
    case XPOWERS_POWERON_512MS: Serial.println("512 Ms");
        break;
    case XPOWERS_POWERON_1S: Serial.println("1 Second");
        break;
    case XPOWERS_POWERON_2S: Serial.println("2 Second");
        break;
    default:
        break;
    }

    Serial.println("===========================================================================");

    bool en;

    // DCDC 120%(130%) high voltage turn off PMIC function
    en = PMU.getDCHighVoltagePowerDowmEn();
    Serial.print("getDCHighVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC1 85% low voltage turn off PMIC function
    en = PMU.getDC1LowVoltagePowerDowmEn();
    Serial.print("getDC1LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC2 85% low voltage turn off PMIC function
    en = PMU.getDC2LowVoltagePowerDowmEn();
    Serial.print("getDC2LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC3 85% low voltage turn off PMIC function
    en = PMU.getDC3LowVoltagePowerDowmEn();
    Serial.print("getDC3LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC4 85% low voltage turn off PMIC function
    en = PMU.getDC4LowVoltagePowerDowmEn();
    Serial.print("getDC4LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC5 85% low voltage turn off PMIC function
    en = PMU.getDC5LowVoltagePowerDowmEn();
    Serial.print("getDC5LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");

    // PMU.setDCHighVoltagePowerDowm(true);
    // PMU.setDC1LowVoltagePowerDowm(true);
    // PMU.setDC2LowVoltagePowerDowm(true);
    // PMU.setDC3LowVoltagePowerDowm(true);
    // PMU.setDC4LowVoltagePowerDowm(true);
    // PMU.setDC5LowVoltagePowerDowm(true);

    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU.disableTSPinMeasure();

    // PMU.enableTemperatureMeasure();

    // Enable internal ADC detection
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();


    /*
      The default setting is CHGLED is automatically controlled by the PMU.
    - XPOWERS_CHG_LED_OFF,
    - XPOWERS_CHG_LED_BLINK_1HZ,
    - XPOWERS_CHG_LED_BLINK_4HZ,
    - XPOWERS_CHG_LED_ON,
    - XPOWERS_CHG_LED_CTRL_CHG,
    * */
    PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);


    pinMode(pmu_irq_pin, INPUT);
    attachInterrupt(pmu_irq_pin, setFlag, FALLING);


    // Disable all interrupts
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    PMU.clearIrqStatus();
    // Enable the required interrupt function
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ | XPOWERS_AXP2101_BAT_REMOVE_IRQ |              //BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ |            //VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |               //POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ           //CHARGE
        // XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ | XPOWERS_AXP2101_PKEY_POSITIVE_IRQ   |   //POWER KEY
    );

    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    // Set constant current charge current limit
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
    // Set stop charging termination current
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);

    // Set the watchdog trigger event type
    PMU.setWatchdogConfig(XPOWERS_AXP2101_WDT_IRQ_TO_PIN);
    // Set watchdog timeout
    PMU.setWatchdogTimeout(XPOWERS_AXP2101_WDT_TIMEOUT_4S);
    // Enable watchdog to trigger interrupt event
    PMU.enableWatchdog();

    PMU.disableWatchdog();

}
