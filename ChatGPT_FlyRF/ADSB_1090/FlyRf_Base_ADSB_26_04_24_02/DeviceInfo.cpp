/*
  Модуль DeviceInfo.cpp
  Назначение:
  - Работа с идентификатором устройства и строкой версии прошивки.

  Основные задачи модуля:
  - Получать уникальный идентификатор микроконтроллера и преобразовывать его в адресный вид.
  - Формировать текстовую версию программы из имени файла проекта.
  - Хранить и отдавать глобальное состояние нашего самолета ThisAircraft.
*/

#include "DeviceInfo.h"
#include <Arduino.h>

static String g_programVersion;  
LocalAircraftState ThisAircraft = {};  

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `DevID_Mapper` и обрабатывает dev id mapper в контексте модуля DeviceInfo.cpp.
//------------------------------------------------------------------------------
uint32_t DevID_Mapper(uint32_t id)
{
    uint8_t id_mask = (id & 0x00FF0000UL) >> 16;

    switch (id_mask)
    {
        case 0xD0:
        case 0xDD:
        case 0xDE:
        case 0xDF:
            id += 0x100000UL;
            break;
        case 0x11:
        case 0x5B:
            id += 0x010000UL;
            break;
        default:
            break;
    }

    return id;
}

//------------------------------------------------------------------------------
// Назначение функции: Возвращает chip id, рассчитанное или считанное по текущему состоянию модуля.
//
//------------------------------------------------------------------------------
uint32_t getChipId()
{
    uint64_t mac = ESP.getEfuseMac();
    uint32_t id = ((uint32_t)((mac >> 40) & 0xFFU)) |
                  ((uint32_t)((mac >> 32) & 0xFFU) << 8) |
                  ((uint32_t)((mac >> 24) & 0xFFU) << 16) |
                  ((uint32_t)((mac >> 16) & 0xFFU) << 24);
    return DevID_Mapper(id);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `DeviceInfo_chipIdHex` и обрабатывает device info chip id hex в контексте модуля DeviceInfo.cpp.
// Локальные переменные: uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// buf — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - buf: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
String DeviceInfo_chipIdHex()
{
    const uint32_t shortId = getChipId() & 0x00FFFFFFUL;
    char buf[7];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    snprintf(buf, sizeof(buf), "%06lX", (unsigned long)shortId);
    return String(buf);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `DeviceInfo_programVersionFromFile` и обрабатывает device info program версию file в контексте модуля DeviceInfo.cpp.
// Локальные переменные: ver_soft — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// slash — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; 
// dot — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
String DeviceInfo_programVersionFromFile(const char* filePath)
{
    String ver_soft = filePath ? String(filePath) : String();
    int slash = ver_soft.lastIndexOf('\\');
    if (slash < 0) slash = ver_soft.lastIndexOf('/');
    if (slash >= 0) ver_soft.remove(0, slash + 1);
    int dot = ver_soft.lastIndexOf('.');
    if (dot > 0) ver_soft.remove(dot);
    return ver_soft;
}

void DeviceInfo_setProgramVersion(const String& version)
{
    g_programVersion = version;
}

const String& DeviceInfo_programVersion()
{
    return g_programVersion;
}
