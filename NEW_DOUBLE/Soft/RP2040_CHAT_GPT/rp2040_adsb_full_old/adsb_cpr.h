#ifndef ADSB_CPR_H
#define ADSB_CPR_H

#include <Arduino.h>
#include "adsb_structs.h"

/**
 * CPR-структура (для одного кадра)
 * odd = true → кадр нечётный
 * odd = false → кадр чётный
 */
struct CprFrame {
    uint32_t lat;   // 17 бит CPR lat
    uint32_t lon;   // 17 бит CPR lon
    bool odd;       // 0 = even, 1 = odd
    uint32_t time;  // время приёма кадра (секунды)
};

/**
 * CPR глобальное декодирование (нужны оба кадра: even + odd).
 * @param even — чётный кадр
 * @param odd — нечётный кадр
 * @param outLat — широта (результат)
 * @param outLon — долгота (результат)
 * @return true, если координаты успешно декодированы
 */
bool cprDecodeGlobal(const CprFrame &even, const CprFrame &odd,
                     double &outLat, double &outLon);

/**
 * CPR локальное декодирование (по одной точке).
 * @param frame — кадр (even или odd)
 * @param refLat — опорная широта (например, приёмника)
 * @param refLon — опорная долгота
 * @param outLat — широта результата
 * @param outLon — долгота результата
 * @return true, если координаты успешно декодированы
 */
bool cprDecodeLocal(const CprFrame &frame, double refLat, double refLon,
                    double &outLat, double &outLon);

#endif // ADSB_CPR_H
