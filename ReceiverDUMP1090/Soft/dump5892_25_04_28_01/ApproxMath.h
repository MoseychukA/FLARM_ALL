/*
 * ApproxMath.h
 */

#ifndef APPROXMATH_H
#define APPROXMATH_H

float atan2_approx(float, float);
float sin_approx(float);
float cos_approx(float);
float approxHypotenuse(float, float);
float CosLat(float);
float InvCosLat(void);

int32_t iatan2_approx( int32_t ns, int32_t ew );
uint32_t iapproxHypotenuse0( int32_t x, int32_t y );
uint32_t iapproxHypotenuse1( int32_t x, int32_t y );

#endif /* APPROXMATH_H */
