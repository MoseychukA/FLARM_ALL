<h1 align = "center">🌟LilyGo LoRa Series🌟</h1>

## **[English](./README.MD) | 中文**

## 快速开始:

1. 将 `lib` 目录中所有文件夹复制到`~/Arduino/libraries`目录内,Windons用户放在 `"我的文档/Arduino/libraries"`
2. 打开对应的示例
3. `T-Beam`用户可以选择`TTGO T-Beam`,`lora32(T3)`用户可以选择`ESP32 Dev Module`,`T-Motion` 用户选择 `Nucleo_l073RZ`,`T3-S3-V1.0`和`T-BeamS3-Core`用户选择`ESP32S3 Dev Module`
4. 在端口中选择板子的端口
5. 请在每个草图的 `utilities.h` 文件的上方根据你的板子型号取消注释,否则编译将报错误
6. 上传

## ⚠ 注意事项 ⚠ 
1. T3S3 V1.1(V1.2 不存在这个问题) SX1280的输出功率设置不能大于2~5dbm，否则可能导致FEM芯片损坏。设置3dbm就已经有20dmb的输出功率

## Product 📷

|                    Product                    |          Product Link           |                       Schematic                        | Corresponding example macro definition |
| :-------------------------------------------: | :-----------------------------: | :----------------------------------------------------: | :------------------------------------: |
|            [T-Beam SX1278/76][1-4]            |         [AliExpress][1]         | [TBeamV0.7][1-1] / [TBeamV1.0][1-2] / [TBeamV1.1][1-3] | LILYGO_TBeam_V0_7 / LILYGO_TBeam_V1_X  |
|             [T-Beam SX1262][1-4]              |         [AliExpress][2]         | [TBeamV0.7][1-1] / [TBeamV1.0][1-2] / [TBeamV1.1][1-3] | LILYGO_TBeam_V0_7 / LILYGO_TBeam_V1_X  |
|           [LoRa32 V1.0 SX1276][3-1]           |         [AliExpress][3]         |                           ❌                            |             LILYGO_T3_V1_0             |
|           [LoRa32 V1.0 SX1278][3-1]           |         [AliExpress][4]         |                           ❌                            |             LILYGO_T3_V1_0             |
|           [LoRa32 V1.3 SX1276][5-2]           |         [AliExpress][5]         |                   [LoRa32 V1.3][5-1]                   |             LILYGO_T3_V1_3             |
|              [LoRa32 V2.0][6-2]               |         [AliExpress][6]         |                   [LoRa32 V2.0][6-1]                   |             LILYGO_T3_V2_0             |
|           [LoRa32 V2.1 & 1.6][7-4]            | [AliExpress][7-1]/[Amazon][7-2] |                 [LoRa32 V2.1_1.6][7-3]                 |             LILYGO_T3_V1_6             |
|             [T-Motion S76G][8-2]              |         [AliExpress][8]         |                    [T-Motion][8-1]                     |          LILYGO_T_MOTION_S76G          |
|            [LoRa32 V1.8 2.4G][9-2]            |         [lilygo.cc][9]          |                [LoRa32 V1.8 2.4G][9-1]                 |             LILYGO_T3_V1_8             |
|             [LoRa T3 S3 V1.0][10]             |         [lilygo.cc][10]         |                [LoRa T3 S3 V1.0][10-1]                 |           LILYGO_T3_S3_V1_0            |
|             [LoRa T3 S3 V1.1][11]             |         [lilygo.cc][11]         |                [LoRa T3 S3 V1.1][11-1]                 |    LILYGO_T3_S3_V1_0  (Same as 1.0)    |
| [T-Beam S3 Supreme][12]/[T-BeamS3 Core][12-3] |        [lilygo.cc][12-1]        |       [T-Beam S3 Supreme & T-Beam S3 Core][12-2]       |      LILYGO_TBeamS3_SUPREME_V3_0       |

[1]: https://pt.aliexpress.com/item/32967228739.html "T-Beam SX1278/76"
[1-1]: schematic/LilyGo_TBeam_V0.7.pdf
[1-2]: schematic/LilyGo_TBeam_V1.0.pdf
[1-3]: schematic/LilyGo_TBeam_V1.1.pdf
[1-4]: assets/image/t-beam_v1.1_pinmap.jpg
[2]: https://pt.aliexpress.com/item/4001287221970.html "T-Beam SX1262"
[3]: https://pt.aliexpress.com/item/32842155838.html "LoRa32 V1.0 SX1276"
[3-1]: assets/image/lora32_v1.0_pinmap.jpg
[4]: https://pt.aliexpress.com/item/32842155838.html "LoRa32 V1.0 SX1278"
[5]: https://pt.aliexpress.com/item/4000628100802.html "LoRa32 V1.3 SX1276"
[5-1]: schematic/T3_V1.3.pdf
[5-2]: assets/image/lora32_v1.3_pinmap.jpg
[6]: https://pt.aliexpress.com/item/32847443952.html "LoRa32 V2.0"
[6-1]: schematic/T3_V2.0.pdf
[6-2]: assets/image/lora32_v2.0_pinmap.jpg
[7-1]: https://pt.aliexpress.com/item/32872078587.html "AliExpress"
[7-2]: https://www.amazon.com/dp/B09SHRWVNB?ref=myi_title_dp "Amazon"
[7-3]: schematic/T3_V1.6.pdf
[7-4]: assets/image/lora32_v1.6.1_pinmap.jpg
[8]: https://www.aliexpress.com/item/4000571051141.html "T-Motion S76G"
[8-1]: schematic/T-Motion.pdf
[8-2]: assets/image/t-motion_v1.0_pinmap.jpg
[9]: https://www.lilygo.cc/products/lora-v1-8-esp32 "LoRa32 V1.8 2.4G"
[9-1]: schematic/T3_V1.8.pdf
[9-2]: assets/image/lora32_v1.8_pinmap.jpg
[10]: https://pt.aliexpress.com/item/1005004627139838.html "LoRa T3 S3 V1.0"
[10-1]: schematic/T3_S3_V1.0.pdf
[11]: NULL
[11-1]: schematic/T3_S3_V1.1.pdf
[12]: assets/image/T-BEAM-S3-Supreme.jpg
[12-3]:assets/image/T-BEAM-S3Core.jpg
[12-1]: NULL
[12-2]: schematic/LilyGo_TBeam_S3_Core_V3.0.pdf


## 示例描述

```
├─ArduinoLoRa       # Only support SX1276/SX1278 radio module
│  ├─LoRaReceiver
│  └─LoRaSender
├─GPS                           # T-Beam GPS demo examples
│  ├─TinyGPS_Example
│  ├─TinyGPS_FullExample
│  ├─TinyGPS_KitchenSink
│  ├─UBlox_BasicNMEARead        # Only support Ublox GNSS Module 
│  ├─UBlox_Recovery             # Only support Ublox GNSS Module 
│  ├─UBlox_OutputRate           # Only support Ublox GNSS Module 
│  └─UBlox_NMEAParsing          # Only support Ublox GNSS Module 
├─OLED                          # OLED demo examples
│  ├─SH1106FontUsage
│  ├─SH1106GraphicsTest
│  ├─SH1106IconMenu
│  ├─SH1106PrintUTF8
│  ├─SSD1306SimpleDemo
│  └─SSD1306UiDemo
├─RadioLibExamples              # RadioLib examples,Support SX1276/78/62/80
│  ├─SX1262
│  │  ├─SX1262_Receive_Interrupt
│  │  └─SX1262_Transmit_Interrupt
│  ├─SX1268
│  │  ├─SX1268_Receive_Interrupt
│  │  └─SX1268_Transmit_Interrupt
│  ├─SX1276
│  │  ├─SX1276_Receive_Interrupt
│  │  └─SX1276_Transmit_Interrupt
│  ├─SX1278
│  │  ├─SX1278_Receive
│  │  ├─SX1278_Receive_Interrupt
│  │  ├─SX1278_Transmit
│  │  └─SX1278_Transmit_Interrupt
│  ├─SX1280
│  │  ├─Ranging
│  │  │  ├─Ranging_Master
│  │  │  └─Ranging_Slave
│  │  ├─SX128x_Receive_Interrupt
│  │  └─SX128x_Transmit_Interrupt
│  └─TBeamAllFunction   # T-Beam all function example,Only tbeamv1.x is supported temporarily
├── Sensor            # Sensor examples , just support t-beams3-supreme
│   ├── BME280_AdvancedsettingsExample
│   ├── BME280_TestExample
│   ├── BME280_UnifiedExample
│   ├── PCF8563_AlarmByUnits
│   ├── PCF8563_SimpleTime
│   ├── PCF8563_TimeLib
│   ├── PCF8563_TimeSynchronization
│   ├── QMC6310_CalibrateExample
│   ├── QMC6310_CompassExample
│   ├── QMC6310_GetDataExample
│   ├── QMC6310_GetPolarExample
│   ├── QMI8658_BlockExample
│   ├── QMI8658_GetDataExample
│   ├── QMI8658_InterruptBlockExample
│   ├── QMI8658_InterruptExample
│   ├── QMI8658_LockingMechanismExample
│   ├── QMI8658_MadgwickAHRS
│   ├── QMI8658_PedometerExample
│   ├── QMI8658_ReadFromFifoExample
│   └── QMI8658_WakeOnMotion
├─SleepTest
└─LoRaWAN                           # OTAA method Join TTN example
    └─OTTA

```

## LoRa 型号分辨

分辨LoRa的型号只需要查看板子上的贴纸,
433MHz则是SX1278，
868MHz则是SX1276，
如果标签写的是SX1262则是SX1262

## 常见问题

1. GPS没有数据输出，[参考GPS恢复出厂](firmware/NMEA_Reset/README.MD)


## 引脚映射

- See [utilities.h](examples/ArduinoLoRa/LoRaReceiver/utilities.h)

## 应用

- [T-Beam SoftRF](https://github.com/lyusupov/SoftRF)
- [T-Beam Paxcounter](https://github.com/cyberman54/ESP32-Paxcounter)
- [T-Beam Meshtastic](https://github.com/meshtastic/Meshtastic-device)

## TTN

接入TTN(The Things Network)使用请参考 [LilyGo_LoRa_Series_Quick_Start](./docs/zh/LilyGo_LoRa_Series_Quick_Start_On_TTN_zh.md)

## 数据手册

- [AXP192](http://www.x-powers.com/en.php/Info/product_detail/article_id/29)
- [AXP2101](./lib/XPowersLib/datasheet/AXP2101_Datasheet_V1.0_en.pdf)
- [SX1262](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1262)
- [SX1278](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1278)
- [SX1276](https://www.semtech.com/products/wireless-rf/lora-transceivers/sx1276)
- [SX1280](https://www.semtech.cn/products/wireless-rf/lora-connect/sx1280)
- [GSP NEO-6/NEO-8](https://www.u-blox.com/en/product/neo-6-series)
- [GSP MAX-M10](https://www.u-blox.com/zh/product/max-m10-series)
- [PCF8563](./lib/SensorsLib/datasheet/PCF8563%20Datasheet%20Rev.11.pdf)
- [QMI8658](./lib/SensorsLib/datasheet/QMI8658A%20Datasheet%20Rev1.0.pdf)
- [QMC6310](./lib/SensorsLib/datasheet/QMC6310%20Datasheet%20Rev.C.pdf)
- [BME280](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/)


## 注意事项

1. LoRa设置输出功率后需要设置最低电流。设置sx1262如下 :

```
    // digitalWrite(LoRa_Busy, LOW);
    // set output power to 10 dBm (accepted range is -17 - 22 dBm)
    if (radio.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        while (true);
    }


    // set over current protection limit to 80 mA (accepted range is 45 - 240 mA)
    // NOTE: set value to 0 to disable overcurrent protection
    if (radio.setCurrentLimit(80) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true);
    }
```
## 注意:

1. 在使用SX1280 PA模块时，建议软件控制1280芯片输出2~5dbm，这样整个模组会输出20dbm的功率；
当1280芯片输出高于2dbm时，整个模组输出功率也只有20dbm；
当1280芯片输出高于5dbm时，整个模组输出功率也只有20dbm，但5dbm给到FEM，会损坏FEM芯片（FEM允许最大输入是5dbm）
如果1280芯片输出低于2dbm，整个模组输出功率20dbm以内。


# FAQ

- GPS备用电源来自于18650电池，如果移除18650电池将无法获得GPS热启动，如果需要使用GPS热启动，请连接18650电池