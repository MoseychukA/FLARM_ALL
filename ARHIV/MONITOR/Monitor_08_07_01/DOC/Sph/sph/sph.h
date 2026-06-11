/* This file was automatically generated.  Do not edit! */
int SphereLinear(double pt1[],double pt2[],double dist13,double dist23,int clockwise,double pt3[]);
int SphereAngular(double pt1[],double pt2[],double azi13,double azi23,double pt3[]);
void SphereDirect(double pt1[],double azi,double dist,double pt2[]);
double CartToSpher(double x[],double y[]);
void Rotate(double x[],double a,int i);
void SpherToCart(double y[],double x[]);
void SphereInverse(double pt1[],double pt2[],double *azi,double *dist);
#define A_E 6371.0
#define Degrees(x) (x * 57.29577951308232)
#define Radians(x) (x / 57.29577951308232)
#define INTERFACE 0
