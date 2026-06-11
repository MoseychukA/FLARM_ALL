#include <stdio.h>
#include <stdlib.h>
#include "sph.h"

int main(int argc, char *argv[])
{
  char buf[1024];
  double pt1[2], pt2[2];
  double lat1, lon1, azi1, dist, azi2;

  while (fgets(buf, 1024, stdin) != NULL) {
    sscanf(buf, "%lf %lf %lf %lf", &lat1, &lon1, &azi1, &dist);
    pt1[0] = Radians(lat1);
    pt1[1] = Radians(lon1);
    SphereDirect(pt1, Radians(azi1), dist / A_E, pt2);
    SphereInverse(pt2, pt1, &azi2, &dist);
    printf("%f\t%f\t%f\n", Degrees(pt2[0]), Degrees(pt2[1]), Degrees(azi2));
  }
  return 0;
}
