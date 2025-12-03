#include <stdio.h>
#include <stdlib.h>
#include "sph.h"

int main(int argc, char *argv[])
{
  char buf[1024];
  double pt1[2], pt2[2], pt3[2];
  double lat1, lon1, lat2, lon2, azi13, azi23;

  while (fgets(buf, 1024, stdin) != NULL) {
    sscanf(buf, "%lf %lf %lf %lf %lf %lf",
	   &lat1, &lon1, &lat2, &lon2, &azi13, &azi23);
    pt1[0] = Radians(lat1);
    pt1[1] = Radians(lon1);
    pt2[0] = Radians(lat2);
    pt2[1] = Radians(lon2);
    if (SphereAngular(pt1, pt2, Radians(azi13), Radians(azi23), pt3))
      puts("\t"); /* Бесконечно много решений на большом круге Q1-Q2 */
    else
      printf("%f\t%f\n", Degrees(pt3[0]), Degrees(pt3[1]));
  }
  return 0;
}
