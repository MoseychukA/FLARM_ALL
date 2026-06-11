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
LocalAircraftState ThisAircraft = {};  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.

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

uint32_t getChipId()
{
    uint64_t mac = ESP.getEfuseMac();
    uint32_t id = ((uint32_t)((mac >> 40) & 0xFFU)) |
                  ((uint32_t)((mac >> 32) & 0xFFU) << 8) |
                  ((uint32_t)((mac >> 24) & 0xFFU) << 16) |
                  ((uint32_t)((mac >> 16) & 0xFFU) << 24);
    return DevID_Mapper(id);
}

// - buf: Буфер, текстовая строка или рабочее сообщение.
String DeviceInfo_chipIdHex()
{
    const uint32_t shortId = getChipId() & 0x00FFFFFFUL;
    char buf[7];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    snprintf(buf, sizeof(buf), "%06lX", (unsigned long)shortId);
    return String(buf);
}

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
