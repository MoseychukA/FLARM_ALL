#include <stdio.h>
#include <stdlib.h>
#include "sph.h"

int main(int argc, char *argv[])
{
  char buf[1024];
  double pt1[2], pt2[2];
  double lat1, lon1, lat2, lon2, azi1, azi2, dist;

  while (fgets(buf, 1024, stdin) != NULL) {
    sscanf(buf, "%lf %lf %lf %lf", &lat1, &lon1, &lat2, &lon2);
    pt1[0] = Radians(lat1);
    pt1[1] = Radians(lon1);
    pt2[0] = Radians(lat2);
    pt2[1] = Radians(lon2);
    SphereInverse(pt2, pt1, &azi2, &dist);
    SphereInverse(pt1, pt2, &azi1, &dist);
    printf("%f\t%f\t%.4f\n", Degrees(azi1), Degrees(azi2), dist * A_E);
  }
  return 0;
}
