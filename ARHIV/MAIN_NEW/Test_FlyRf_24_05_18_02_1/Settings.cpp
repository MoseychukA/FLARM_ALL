#include "Settings.h"

static union {
    uint8_t efuse_mac[6];
    uint64_t chipmacid;
};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Settings
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
SettingsClass Settings;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{
 
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setup()
{
#if !defined(SOFTRF_ADDRESS)

    esp_err_t ret = ESP_OK;
    uint8_t null_mac[6] = { 0 };

    ret = esp_efuse_mac_get_custom(efuse_mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get base MAC address from BLK3 of EFUSE error (%s)", esp_err_to_name(ret));
        /* If get custom base MAC address error, the application developer can decide what to do:
         * abort or use the default base MAC address which is stored in BLK0 of EFUSE by doing
         * nothing.
         */

        ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
        chipmacid = ESP.getEfuseMac();
    }
    else {
        if (memcmp(efuse_mac, null_mac, 6) == 0) {
            ESP_LOGI(TAG, "Use base MAC address which is stored in BLK0 of EFUSE");
            chipmacid = ESP.getEfuseMac();
        }
    }
#endif /* SOFTRF_ADDRESS */

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void SettingsClass::update()
{
  
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::draw()
{


}


long SettingsClass::getCurrentLatitude()
{
    return curLatitude;
}
void SettingsClass::setCurrentLatitude(long lat)
{
    curLatitude = lat;
}
long SettingsClass::getLCurrentLongitude()
{
    return curLongitude;
}
void SettingsClass::setLCurrentLongitude(long lon)
{
    curLongitude = lon;
}

void SettingsClass::setNumSat(uint8_t n_sat)
{
    num_sat = n_sat;
}
uint8_t SettingsClass::getNumSat()
{
    return num_sat;
}


void SettingsClass::setAltitude(float alt)
{
    Altitude_m = alt;
}
float SettingsClass::getAltitude()
{
    return Altitude_m;
}

void SettingsClass::setnavSystem(char navSys)
{
    satnavSystem = navSys;
}
char SettingsClass::getnavSystem()
{
    return satnavSystem;
}

int SettingsClass::countTest()
{
    return count;
}

void  SettingsClass::saveVer(String ver)
{

    Current_version = ver;

}

String  SettingsClass::getVer()
{

    return Current_version;
}

uint32_t SettingsClass::DevID_Mapper(uint32_t id)
{
    uint8_t id_mask = (id & 0x00FF0000UL) >> 16;

    switch (id_mask)
    {
        /* remap address to avoid overlapping with congested FLARM range */
    case 0xD0:
    case 0xDD:
    case 0xDE:
    case 0xDF:
        id += 0x100000;
        break;
        /* remap 11xxxx addresses to avoid overlapping with congested Skytraxx range */
    case 0x11:
        /*
         * OGN 0.2.8+ does not decode 'Air V6' traffic when leading byte of 24-bit Id is 0x5B
         */
    case 0x5B:
        id += 0x010000;
        break;

    default:
        break;
    }

    return id;
}



uint32_t SettingsClass::ESP32_getChipId()
{
#if !defined(SOFTRF_ADDRESS)

    uint32_t id = (uint32_t)efuse_mac[5] | ((uint32_t)efuse_mac[4] << 8) | \
        ((uint32_t)efuse_mac[3] << 16) | ((uint32_t)efuse_mac[2] << 24);

    return Settings.DevID_Mapper(id);
#else
    return (SOFTRF_ADDRESS & 0xFFFFFFFFU);
#endif /* SOFTRF_ADDRESS */
}


