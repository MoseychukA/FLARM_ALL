#include "FlyRfSpi.h"

extern "C" void FlyRfSpiSetup()
{
}

extern "C" bool FlyRfSpiLock(uint32_t timeoutMs)
{
    (void)timeoutMs;
    return true;
}

extern "C" void FlyRfSpiUnlock()
{
}
