

const int ledPin =  35;// the number of the LED pin

int ledState = LOW;             // ledState used to set the LED


void setup() 
{
  Serial1.begin(9600);
  pinMode(ledPin, OUTPUT);
}

void loop() 
{
  
  Serial1.println("$GPTXT,01,01,01,ANTENNA OK*35");
  Serial1.println("$GNGGA,103315.000,5557.30652,N,03713.92370,E,1,06,6.7,232.6,M,0.0,M,,*7C");
  Serial1.println("$GNGLL,5557.30652,N,03713.92370,E,103315.000,A,A*4B");
  Serial1.println("$GPGSA,A,3,07,08,09,194,,,,,,,,,7.9,6.7,4.1*02");
  Serial1.println("$BDGSA,A,3,09,40,,,,,,,,,,,7.9,6.7,4.1*24");
  Serial1.println("$GPGSV,3,1,12,05,40,283,,07,61,084,20,08,25,078,30,09,20,133,28*7D");
  Serial1.println("$GPGSV,3,2,12,13,31,295,,14,31,190,,18,10,341,,20,33,242,*72");
  Serial1.println("$GPGSV,3,3,12,22,12,197,,27,20,043,18,30,80,234,,194,18,056,24*41");
  Serial1.println("$BDGSV,1,1,02,09,30,082,23,40,34,089,26*6D");
  Serial1.println("$GNRMC,103315.000,A,5557.30652,N,03713.92370,E,0.00,283.53,131124,,,A*77");
  Serial1.println("$GNVTG,283.53,T,,M,0.00,N,0.00,K,A*2C");
  Serial1.println("$GNZDA,103315.000,13,11,2024,00,00*4B");
  
  delay(5 00);
 
    if (ledState == LOW) 
    {
      ledState = HIGH;
    } 
    else 
    {
      ledState = LOW;
    }
    digitalWrite(ledPin, ledState);
}
