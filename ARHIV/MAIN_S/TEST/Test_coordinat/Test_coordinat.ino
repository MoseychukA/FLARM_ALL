/*
//$lat - текущая широта, $lng - текущая долгота
//$dist - расстояние между искомыми точками
//$ang - азимут на искомую точку

$newlat=sin($lat * PI / 180)*cos($dist * PI / 180)+cos($lat * PI / 180)*sin($dist * PI / 180)*cos($ang * PI / 180);
$newlat=$lat+(asin($newlat));

$newlng=sin($dist * PI / 180)*sin($ang * PI / 180)/(cos($lat * PI / 180)*cos($dist * PI / 180)-sin($lat * PI / 180)*sin($dist * PI / 180)*cos($ang * PI / 180));
$newlng=$lng+(atan($newlng));



LAT1 = LAT + L * COS(AZIMUT * PI / 180) / (6371000 * PI / 180)
LON1 = LON + L * SIN(AZIMUT * PI / 180) / COS(LAT * PI / 180) / (6371000 * PI / 180)


LAT1 = LAT + 0.718*COS(AZIMUT*PI/180)*L
LON1 = SIN(AZIMUT*PI/180)*L

*/


float alien_lat0 = 55.958328;  // 
float alien_lon0 = 37.245259;


float coord[2];

void coordinate_calculation(float lat, float lon, float course, int distance)
{

    float LAT = lat;
    float LON = lon;
    float LAT1 = 0;
    float LON1 = 0;
    float AZIMUT = course;
    int L = distance;

    LAT1 = LAT + L * cos(AZIMUT * PI / 180) / (6371000 * PI / 180);
    LON1 = LON + L * sin(AZIMUT * PI / 180) / cos(LAT * PI / 180) / (6371000 * PI / 180);

    coord[0] = LAT1;
    coord[1] = LON1;
  //  return coord;

}

//float lat

void setup() {
 
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // prints title with ending line break
  Serial.println("Setup start");


  coordinate_calculation(alien_lat0, alien_lon0, 90, 5000);

  delay(2000);

  Serial.println(coord[0],6);
  Serial.println(coord[1],6);

}


void loop() 
{
 
    //if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 2000) {
    //    // ThisAircraft.latitude  = pgm_read_float( &txrx_test_positions[pos_ndx][0]);
    //    // ThisAircraft.longitude = pgm_read_float( &txrx_test_positions[pos_ndx][1]);
    //    ThisAircraft.latitude = 56.026725;
    //    ThisAircraft.longitude = 38.291524;
    //    pos_ndx = (pos_ndx + 1) % TXRX_TEST_NUM_POSITIONS;
    //    TxPosUpdMarker = millis();
    //}


}
