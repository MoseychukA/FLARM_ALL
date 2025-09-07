#pragma once
#include <stdint.h>
void storageInit();
bool storageLoad();
bool storageSave();
// параметры
void storageSetProfile(int id);
int  storageGetProfile();
void storageSetOutputFmt(int fmt);
int  storageGetOutputFmt();
void storageSetIcao(uint32_t icao);
uint32_t storageGetIcao();
void storageSetMinAlt(int ft);
int  storageGetMinAlt();
void storageSetMaxAlt(int ft);
int  storageGetMaxAlt();