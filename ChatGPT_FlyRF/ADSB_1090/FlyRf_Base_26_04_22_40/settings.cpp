#include "settings.h"

SettingsManager settings_manager;

bool SettingsManager::Load()
{
    return Apply();
}

bool SettingsManager::Save()
{
    return true;
}

bool SettingsManager::Apply()
{
    return true;
}
