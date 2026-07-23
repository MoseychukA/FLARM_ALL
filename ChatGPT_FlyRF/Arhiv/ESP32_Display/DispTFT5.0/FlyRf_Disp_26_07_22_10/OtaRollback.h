#pragma once

void OtaRollback_begin();
void OtaRollback_setSystemReady();
void OtaRollback_loop();
bool OtaRollback_pendingVerification();
bool OtaRollback_bootConfirmed();
