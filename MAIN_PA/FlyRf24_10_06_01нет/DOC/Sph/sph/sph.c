#include <stdlib.h>
#include <math.h>
#include "sph.h"

#if INTERFACE

#define Radians(x) (x / 57.29577951308232)
#define Degrees(x) (x * 57.29577951308232)
#define A_E 6371.0

#endif /* INTERFACE */

void SphereInverse(double pt1[], double pt2[], double *azi, double *dist)
{
  double x[3], pt[2];

  SpherToCart(pt2, x);
  Rotate(x, pt1[1], 2);
  Rotate(x, M_PI_2 - pt1[0], 1);
  CartToSpher(x, pt);
  *azi = M_PI - pt[1];
  *dist = M_PI_2 - pt[0];

  return;
}

void SphereDirect(double pt1[], double azi, double dist, double pt2[])
{
  double pt[2], x[3];

  pt[0] = M_PI_2 - dist;
  pt[1] = M_PI - azi;
  SpherToCart(pt, x);
  Rotate(x, pt1[0] - M_PI_2, 1);
  Rotate(x, -pt1[1], 2);
  CartToSpher(x, pt2);

  return;
}

int SphereAngular(double pt1[], double pt2[], double azi13, double azi23,
		  double pt3[])
{
  double azi12, dist12, azi21, dist13;
  double cos_beta1, sin_beta1, cos_beta2, sin_beta2, cos_dist12, sin_dist12;

  SphereInverse(pt2, pt1, &azi21, &dist12);
  SphereInverse(pt1, pt2, &azi12, &dist12);
  cos_beta1 = cos(azi13 - azi12);
  sin_beta1 = sin(azi13 - azi12);
  cos_beta2 = cos(azi21 - azi23);
  sin_beta2 = sin(azi21 - azi23);
  cos_dist12 = cos(dist12);
  sin_dist12 = sin(dist12);

  if (sin_beta1 == 0. && sin_beta2 == 0.)     // Решение - любая точка
    return -1;				      //   на большом круге Q1-Q2.
  else if (sin_beta1 == 0.) {
    pt3[0] = pt2[0];			      // Решение - точка Q2.
    pt3[1] = pt2[1];
    return 0;
  } else if (sin_beta2 == 0.) {		      // Решение - точка Q1.
    pt3[0] = pt1[0];
    pt3[1] = pt1[1];
    return 0;
  } else if (sin_beta1 * sin_beta2 < 0.) {    // Лучи Q1-Q3 и Q2-Q3 направлены
    if (fabs(sin_beta1) >= fabs(sin_beta2)) { //   в разные полусферы.
      cos_beta2 = -cos_beta2;		      // Выберем ближайшее решение:
      sin_beta2 = -sin_beta2;		      //   развернём луч Q2-Q3 на 180°;
    } else {				      //     иначе
      cos_beta1 = -cos_beta1;		      //   развернём луч Q1-Q3 на 180°.
      sin_beta1 = -sin_beta1;
    }
  }
  dist13 = atan2(fabs(sin_beta2) * sin_dist12,
		 cos_beta2 * fabs(sin_beta1)
		 + fabs(sin_beta2) * cos_beta1 * cos_dist12);
  SphereDirect(pt1, azi13, dist13, pt3);

  return 0;
}

int SphereLinear(double pt1[], double pt2[], double dist13, double dist23,
		 int clockwise, double pt3[])
{
  double azi12, dist12, azi13;
  double cos_beta1;

  if (dist13 == 0.) {			      // Решение - точка Q1.
    pt3[0] = pt1[0];
    pt3[1] = pt1[1];
    return 0;
  } else if (dist23 == 0.) {		      // Решение - точка Q2.
    pt3[0] = pt2[0];
    pt3[1] = pt2[1];
    return 0;
  }

  SphereInverse(pt1, pt2, &azi12, &dist12);
  cos_beta1 = (cos(dist23) - cos(dist12) * cos(dist13))
    / (sin(dist12) * sin(dist13));
  if (fabs(cos_beta1) > 1.)		      // Решение не существует.
    return -1;
  azi13 = clockwise ? azi12 + acos(cos_beta1) : azi12 - acos(cos_beta1);
  SphereDirect(pt1, azi13, dist13, pt3);

  return 0;
}

void Rotate(double x[], double a, int i)
{
  double c, s, xj;
  int j, k;

  j = (i + 1) % 3;
  k = (i - 1) % 3;
  c = cos(a);
  s = sin(a);
  xj = x[j] * c + x[k] * s;
  x[k] = -x[j] * s + x[k] * c;
  x[j] = xj;

  return;
}

void SpherToCart(double y[], double x[])
{
  double p;

  p = cos(y[0]);
  x[2] = sin(y[0]);
  x[1] = p * sin(y[1]);
  x[0] = p * cos(y[1]);

  return;
}

double CartToSpher(double x[], double y[])
{
  double p;

  p = sqrt(x[0] * x[0] + x[1] * x[1]);
  y[1] = atan2(x[1], x[0]);
  y[0] = atan2(x[2], p);

  return sqrt(p * p + x[2] * x[2]);
}
