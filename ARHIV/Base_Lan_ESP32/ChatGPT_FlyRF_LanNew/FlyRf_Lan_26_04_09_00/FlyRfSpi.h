#pragma once
#include <Arduino.h>

extern "C" void FlyRfSpiSetup();
extern "C" bool FlyRfSpiLock(uint32_t timeoutMs = 50);
extern "C" void FlyRfSpiUnlock();

struct FlyRfSpiGuard
{
    bool locked;
    explicit FlyRfSpiGuard(uint32_t timeoutMs = 50) : locked(FlyRfSpiLock(timeoutMs)) {}
    ~FlyRfSpiGuard() { if (locked) FlyRfSpiUnlock(); }
    operator bool() const { return locked; }
};
