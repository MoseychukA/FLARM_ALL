#include <stdio.h>
#include <stdlib.h>
#include "sph.h"

int main(int argc, char *argv[])
{
  char buf[1024];
  double pt1[2], pt2[2], pt3[2];
  double lat1, lon1, lat2, lon2, dist13, dist23;
  int clockwise;

  while (fgets(buf, 1024, stdin) != NULL) {
    sscanf(buf, "%lf %lf %lf %lf %lf %lf %d",
	   &lat1, &lon1, &lat2, &lon2, &dist13, &dist23, &clockwise);
    pt1[0] = Radians(lat1);
    pt1[1] = Radians(lon1);
    pt2[0] = Radians(lat2);
    pt2[1] = Radians(lon2);
    if (SphereLinear(pt1, pt2, dist13 / A_E, dist23 / A_E, clockwise, pt3))
      puts("\t"); /* Решений нет */
    else
      printf("%f\t%f\n", Degrees(pt3[0]), Degrees(pt3[1]));
  }
  return 0;
}
