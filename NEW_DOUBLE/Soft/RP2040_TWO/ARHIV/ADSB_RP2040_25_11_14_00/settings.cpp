#include "settings.h"

#include "adsbee.h"
#include "comms.h"
#include "hardware/flash.h"
#include "bsp.h"


bool SettingsManager::Load() 
{
 
    Apply();

    return true;
}

bool SettingsManager::Save()
{
    //settings.receiver_enabled = adsbee.Receiver1090IsEnabled();
    //settings.tl_mv = adsbee.GetTLMilliVolts();
    //settings.bias_tee_enabled = adsbee.BiasTeeIsEnabled();
    //settings.watchdog_timeout_sec = adsbee.GetWatchdogTimeoutSec();

    // Save log level.
    settings.log_level = comms_manager.log_level;

    return true;
}


bool SettingsManager::Apply() 
{
    //adsbee.SetReceiver1090Enable(settings.receiver_enabled);
    //adsbee.SetTLMilliVolts(settings.tl_mv);
    //adsbee.SetBiasTeeEnable(settings.bias_tee_enabled);
   // adsbee.SetWatchdogTimeoutSec(settings.watchdog_timeout_sec); //Запуск сторожевого таймера

    // Apply log level.
    comms_manager.log_level = settings.log_level;

    return true;  
}
