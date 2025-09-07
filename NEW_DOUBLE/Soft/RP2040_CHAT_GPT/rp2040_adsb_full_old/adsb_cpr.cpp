#include "adsb_cpr.h"
#include <math.h>

/**
 * Таблица NL(lat) из ICAO Annex 10.
 * Возвращает количество зон долготы в данной широте.
 */
static int cprNL(double lat) {
    if (lat < 0) lat = -lat;  // только абсолютное значение
    if (lat < 10.47047130) return 59;
    if (lat < 14.82817437) return 58;
    if (lat < 18.18626357) return 57;
    if (lat < 21.02939493) return 56;
    if (lat < 23.54504487) return 55;
    if (lat < 25.82924707) return 54;
    if (lat < 27.93898710) return 53;
    if (lat < 29.91135686) return 52;
    if (lat < 31.77209708) return 51;
    if (lat < 33.53993436) return 50;
    if (lat < 35.22899598) return 49;
    if (lat < 36.85025108) return 48;
    if (lat < 38.41241892) return 47;
    if (lat < 39.92256684) return 46;
    if (lat < 41.38651832) return 45;
    if (lat < 42.80914012) return 44;
    if (lat < 44.19454951) return 43;
    if (lat < 45.54626723) return 42;
    if (lat < 46.86733252) return 41;
    if (lat < 48.16039128) return 40;
    if (lat < 49.42776439) return 39;
    if (lat < 50.67150166) return 38;
    if (lat < 51.89342469) return 37;
    if (lat < 53.09516153) return 36;
    if (lat < 54.27817472) return 35;
    if (lat < 55.44378444) return 34;
    if (lat < 56.59318756) return 33;
    if (lat < 57.72747354) return 32;
    if (lat < 58.84763776) return 31;
    if (lat < 59.95459277) return 30;
    if (lat < 61.04917774) return 29;
    if (lat < 62.13216659) return 28;
    if (lat < 63.20427479) return 27;
    if (lat < 64.26616523) return 26;
    if (lat < 65.31845310) return 25;
    if (lat < 66.36171008) return 24;
    if (lat < 67.39646774) return 23;
    if (lat < 68.42322022) return 22;
    if (lat < 69.44242631) return 21;
    if (lat < 70.45451075) return 20;
    if (lat < 71.45986473) return 19;
    if (lat < 72.45884545) return 18;
    if (lat < 73.45177442) return 17;
    if (lat < 74.43893416) return 16;
    if (lat < 75.42056257) return 15;
    if (lat < 76.39684391) return 14;
    if (lat < 77.36789461) return 13;
    if (lat < 78.33374083) return 12;
    if (lat < 79.29428225) return 11;
    if (lat < 80.24923213) return 10;
    if (lat < 81.19801349) return 9;
    if (lat < 82.13956981) return 8;
    if (lat < 83.07199445) return 7;
    if (lat < 83.99173563) return 6;
    if (lat < 84.89166191) return 5;
    if (lat < 85.75541621) return 4;
    if (lat < 86.53536998) return 3;
    if (lat < 87.00000000) return 2;
    return 1;
}

/**
 * Вспомогательная функция mod() с корректной работой для отрицательных значений.
 */
static inline int mod(int a, int b) {
    return ((a % b) + b) % b;
}

/**
 * CPR decode: глобальный метод (even + odd).
 */
bool cprDecodeGlobal(const CprFrame &even, const CprFrame &odd,
                     double &outLat, double &outLon) {
    if (abs((long)(even.time - odd.time)) > 10) {
        return false;  // кадры должны быть в пределах 10 секунд
    }

    double dLatEven = 360.0 / 60.0;
    double dLatOdd  = 360.0 / 59.0;

    int j = floor((59 * even.lat - 60 * odd.lat) / pow(2, 17) + 0.5);

    double rlatEven = dLatEven * (mod(j, 60) + (double)even.lat / pow(2, 17));
    double rlatOdd  = dLatOdd  * (mod(j, 59) + (double)odd.lat / pow(2, 17));

    if (rlatEven >= 270) rlatEven -= 360;
    if (rlatOdd  >= 270) rlatOdd  -= 360;

    int nlEven = cprNL(rlatEven);
    int nlOdd  = cprNL(rlatOdd);
    if (nlEven != nlOdd) return false;

    int ni = (even.time > odd.time) ? nlEven : nlOdd;
    double m = floor(((double)even.lon * (nlOdd - 1) -
                      (double)odd.lon * nlEven) / pow(2, 17) + 0.5);

    double lon = (360.0 / ni) * (mod(m, ni) +
                  (double)((even.time > odd.time) ? even.lon : odd.lon) / pow(2, 17));
    if (lon > 180) lon -= 360;

    outLat = (even.time > odd.time) ? rlatEven : rlatOdd;
    outLon = lon;
    return true;
}

/**
 * CPR decode: локальный метод (по одной точке + опорная позиция).
 */
bool cprDecodeLocal(const CprFrame &frame, double refLat, double refLon,
                    double &outLat, double &outLon) {
    double dLat = (frame.odd) ? 360.0 / 59.0 : 360.0 / 60.0;
    int j = floor(refLat / dLat) +
            floor(0.5 + fmod(refLat, dLat) / dLat - (double)frame.lat / pow(2, 17));
    double rlat = dLat * (j + (double)frame.lat / pow(2, 17));

    if (rlat >= 270) rlat -= 360;

    int nl = cprNL(rlat) - (frame.odd ? 1 : 0);
    if (nl < 1) nl = 1;

    double dLon = 360.0 / nl;
    int m = floor(refLon / dLon) +
            floor(0.5 + fmod(refLon, dLon) / dLon - (double)frame.lon / pow(2, 17));
    double rlon = dLon * (m + (double)frame.lon / pow(2, 17));

    if (rlon > 180) rlon -= 360;

    outLat = rlat;
    outLon = rlon;
    return true;
}
