/*
  Модуль OTA.h
  Назначение:
  - Публичный интерфейс OTA-обновления.

  Что содержит файл:
  - Объявления функций запуска и обслуживания OTA-подсистемы.
*/


#ifndef OTAHELPER_H
#define OTAHELPER_H

#define APORT 8266 ///< Port for OTA update

void OTA_setup(void);
void OTA_loop(void);

#endif /* OTAHELPER_H */
