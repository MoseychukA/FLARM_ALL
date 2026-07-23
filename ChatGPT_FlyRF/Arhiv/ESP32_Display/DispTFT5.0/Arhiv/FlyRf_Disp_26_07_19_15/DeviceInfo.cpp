#include "DeviceInfo.h"
#include <esp_system.h>

static String g_programVersion;
LocalAircraftState ThisAircraft = {};

uint32_t DevID_Mapper(uint32_t id) { return id & 0x00FFFFFFUL; }
uint32_t getChipId()
{
    uint64_t mac = ESP.getEfuseMac();
    return (uint32_t)(mac & 0x00FFFFFFUL);
}
void DeviceInfo_setProgramVersion(const String& version) { g_programVersion = version; }
const String& DeviceInfo_programVersion() { return g_programVersion; }
String DeviceInfo_programVersionFromFile(const char* filePath)
{
    if (!filePath) return String("FlyRf_Disp_26_04_26_08");
    String s(filePath);
    int slash = s.lastIndexOf('/');
    int bslash = s.lastIndexOf('\\');
    int pos = max(slash, bslash);
    if (pos >= 0) s = s.substring(pos + 1);
    if (s.endsWith(".ino")) s.remove(s.length() - 4);
    return s;
}
String DeviceInfo_chipIdHex()
{
    char buf[9];
    snprintf(buf, sizeof(buf), "%06lX", (unsigned long)getChipId());
    return String(buf);
}
