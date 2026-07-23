/*
  Модуль System.h
  Назначение:
  - Публичный интерфейс верхнего уровня системы.

  Что содержит файл:
  - Объявления функций общей инициализации и главного цикла.
  - Функцию получения курса, используемого для отображения.
*/

#pragma once

void SystemSetup();
void SystemLoop();
void SystemEnterOtaMode();
bool SystemOtaPendingVerification();
bool SystemOtaBootConfirmed();
float SystemDisplayCourseDeg();
