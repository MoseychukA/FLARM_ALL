/*
  ASCII table

  Prints out byte values in all possible formats:
  - as raw binary values
  - as ASCII-encoded decimal, hex, octal, and binary values

  For more on ASCII, see http://www.asciitable.com and http://en.wikipedia.org/wiki/ASCII

  The circuit: No external hardware needed.

  created 2006
  by Nicholas Zambetti <http://www.zambetti.com>
  modified 9 Apr 2012
  by Tom Igoe

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/ASCIITable
*/

// constants won't change. Used here to set a pin number:
const int ledPin =  PC13;// the number of the LED pin

// Variables will change:
int ledState = LOW;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)


void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  Serial1.begin(115200);
 // while (!Serial) {}
  pinMode(ledPin, OUTPUT);
  // prints title with ending line break
  Serial.println("ASCII Table ~ Character Map");
  Serial1.println("ASCII Table ~ Character Map");
}

// first visible ASCIIcharacter '!' is number 33:
int thisByte = 33;
// you can also write ASCII characters in single quotes.
// for example, '!' is the same as 33, so you could also use this:
// int thisByte = '!';

void loop() {
  // prints value unaltered, i.e. the raw binary version of the byte.
  // The Serial Monitor interprets all bytes as ASCII, so 33, the first number,
  // will show up as '!'
  Serial.write(thisByte);
  Serial1.write(thisByte);
  Serial.print(", dec: ");
  Serial1.print(", dec: ");
  // prints value as string as an ASCII-encoded decimal (base 10).
  // Decimal is the default format for Serial.print() and Serial.println(),
  // so no modifier is needed: 
  Serial.print(thisByte);
  Serial1.print(thisByte);
  // But you can declare the modifier for decimal if you want to.
  // this also works if you uncomment it:

  // Serial.print(thisByte, DEC);


  Serial.print(", hex: ");
  Serial1.print(", hex: ");
  // prints value as string in hexadecimal (base 16):
  Serial1.print(thisByte, HEX);

  Serial.print(", oct: ");
  Serial1.print(", oct: ");
  // prints value as string in octal (base 8);
  Serial.print(thisByte, OCT);
  Serial1.print(thisByte, OCT);

  Serial.print(", bin: ");
  Serial1.print(", bin: ");
  // prints value as string in binary (base 2) also prints ending line break:
  Serial.println(thisByte, BIN);
  Serial1.println(thisByte, BIN);
  // if printed last visible character '~' or 126, stop:
  if (thisByte == 126) 
  {  
    thisByte = 33;
  }
  // go on to the next character
  thisByte++;
  // here is where you'd put code that needs to be running all the time.

  // check to see if it's time to blink the LED; that is, if the difference
  // between the current time and last time you blinked the LED is bigger than
  // the interval at which you want to blink the LED.
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
  }
}
