/*
  Mouse Wiggler - Anti Screen Saver

  This circuit uses the HID function of a Arduino "Micro Pro" to block screen savers,
  when the room light is on. So that when the room light is off the screen saver
  is allowed.

  In general the Micro Pro sends out a mouse movement periodically of the lowest
  increament (1) and immediately followed by its negative (-1), as not to be noticable
  by a user, but prevent the screen saver from activating.

  A voltage divider using a 2.2K resistor and a Photo Cell. See the attached Schematic
  and Layout drives the Analog Input that is smoothed (averaged). When a -10% change is
  detected the mouse wiggle is disabled. Conversely when a +10% change is detected
  the wiggle is enabled. This discriminates between ambient sun light and that of room lights.

  The circuit:
  - 2.2K Resistor attached between digital pin D5 to A0.
  - CDS (photo cell) attached between analog pin A0 and digital pin 15
  - (Optional) LED between D2 and GND. On indicates light is present. Off is no light.

  created 31 Aug 2019
  by Michael P. Flaga

  disclaimer:
  The use of this software is done at your own discretion and risk and with agreement that
  you will be solely responsible for any damage to your computer system or loss of data that
  results from such activities. You are solely responsible for adequate protection, security
  and backup of data and equipment used in connection with any of this software, and we will
  not be liable for any damages that you may suffer in connection with using, modifying or
  distributing any of this software.
*/

#include <TimerOne.h>
#include <TimerThree.h>
#include "Mouse.h"

// the setup routine runs once when you press reset:
#define photoCellVCC       A1 // one leg of the photo cell.
#define photoCellSense     A0 // other leg of photo cell joined with Devider Resistor.
#define photoCellGND       15 // other leg of devider resistor.
#define margin             10 // percent of light change to toggle wiggle on or off.
#define led_pin             2 // indicator that lights have been detected. when running as 3.3v, then no resistor is acceptable.
#define wigglePeriod       10 // period in seconds between wiggles.
#define SizeOfAnalogArray 100 // size of buffer to average the data samples.
unsigned int AnalogArray[SizeOfAnalogArray];
unsigned int AnalogArrayPosition = 0;

bool        lightStatus = true; // indicator of current status.
signed long priorSum;          // value to determine change in signal.
int         wiggleSec = 0;     // value tracking seconds

void setup()
{
  // assign pins to drive high and low power of voltage devider.
  digitalWrite(photoCellGND, LOW);
  pinMode(photoCellGND, OUTPUT);

  pinMode(photoCellSense, INPUT);

  digitalWrite(photoCellVCC, HIGH);
  pinMode(photoCellVCC, OUTPUT);

  digitalWrite(led_pin, LOW);
  pinMode(led_pin, OUTPUT);

  // initialize array with sensor data, so average is good.
  for (int counter = 0 ; counter < SizeOfAnalogArray ; counter++)
    AnalogArray[counter++] = analogRead(photoCellSense);

  // setup timer 1 to periodically collect the Photo Cells light signal
  Timer1.initialize((1 * 1000000 /*1uS*/) / SizeOfAnalogArray); // samples per second
  Timer1.attachInterrupt(readAnalogIntoArray);

  // setup timer 3 to periodically send wiggle
  // note max timer interval is 8388480 or 8.3. see https://playground.arduino.cc/Code/Timer1/
  Timer3.initialize(1000000 /*1uS*/);
  Timer3.attachInterrupt(sendWiggle); //

  Serial.begin(115200);
  // only needed for debugging from begining of boot up.
  //   while (!Serial) {
  //    ; // wait for serial port to connect. Needed for native USB port only
  //  }

  // initialize mouse control:
  Mouse.begin();
}

void readAnalogIntoArray(void)
{
  AnalogArray[AnalogArrayPosition++] = analogRead(photoCellSense);

  AnalogArrayPosition = AnalogArrayPosition % SizeOfAnalogArray; // wrap the position, if needed
  // if just wrapped then check for change in average since last check.
  if (!AnalogArrayPosition)
  {
    String      toSerial = ""; // initialize print message.

    // sum up the current avarage of the buffer.
    signed long sum      =  0;
    for (int counter = 0 ; counter < SizeOfAnalogArray ; counter++)
      sum += (signed long) AnalogArray[counter] ;

    signed long diff = priorSum - sum;
    signed long percent = 1 - (diff * 100 / priorSum);

    // print misc knowledge
    toSerial += "("; toSerial += millis(); toSerial += "ms) "; 
    toSerial += "ADC Prior: "; toSerial += priorSum;
    toSerial += "\tADC Channel: "; toSerial += sum;
    toSerial += "\tdiff: "; toSerial += diff;
    toSerial += "\tpercent: "; toSerial += percent;

    // store the current average time for next check in.
    priorSum = sum;

    if (Serial)
      Serial.println(toSerial);

    if (percent > margin) {
      if (Serial)
        Serial.println("Lights Detected ON");
      lightStatus = true;
    }
    else if (percent < -margin) {
      if (Serial)
        Serial.println("Lights Detected OFF");
      lightStatus = false;
    }

  }
}

void sendWiggle(void)
{
  if (!(wiggleSec++ % wigglePeriod)) {
    wiggleSec = 1; // avoid rollover remainder problem

    if (lightStatus) {
      if (Serial)
        Serial.println("Need to wiggle the mouse");

      Mouse.move(1, 1, 0);
      delay(1);
      Mouse.move(-1, -1, 0);
      digitalWrite(led_pin, HIGH);
    }
    else {
      if (Serial)
        Serial.println("Do not wiggle the mouse");

      digitalWrite(led_pin, LOW);
    }
  }
}


void loop()
{
  // do nothing, let interrupts handle it all
}
