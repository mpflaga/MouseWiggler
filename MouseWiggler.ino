/*
  Mouse Wiggler - Anti Screen Saver

  This circuit uses the HID function of a Arduino "Micro Pro" to block screen savers,
  when the room light is on. So that when the room light is off the screen saver
  is allowed.

  In general the Micro Pro sends out a mouse movement periodically of the lowest
  increament (1) and immediately followed by its negative (-1), as not to be noticable
  by a user, but prevent the screen saver from activating.

  A voltage divider using a 2.2K resistor and a Photo Cell. See the attached Schematic
  and Layout drives the Analog Input that is smoothed (averaged). The system uses a
  rate-of-change detection algorithm to distinguish between sudden changes (light switch)
  and gradual changes (sunrise/sunset/clouds). Only sudden changes above a threshold
  trigger wiggle enable/disable, while gradual ambient light changes are ignored.

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

// Hardware Pin Definitions
/** @brief VCC pin for photocell voltage divider */
#define photoCellVCC       A1

/** @brief Analog input pin connected to photocell sense point */
#define photoCellSense     A0

/** @brief Ground pin for photocell voltage divider */
#define photoCellGND       15

/** @brief LED indicator pin (HIGH when light detected, LOW when dark) */
#define led_pin             2

// Detection Algorithm Parameters
/**
 * @brief Percentage change threshold for detecting sudden light changes
 * @details Light changes exceeding this percentage are considered significant.
 *          Typical value: 15% for distinguishing room lights from ambient changes.
 */
#define SUDDEN_CHANGE_THRESHOLD  15

/**
 * @brief Rate of change threshold (percent per sample period)
 * @details Used to distinguish sudden changes (light switch) from gradual changes (sunset/sunrise).
 *          Changes faster than this rate trigger wiggle state changes.
 *          Typical value: 5% per sample period.
 */
#define CHANGE_RATE_THRESHOLD    5

// Timing and Buffer Configuration
/** @brief Base period in seconds between mouse wiggle movements */
#define wigglePeriod       10

/** @brief Maximum random variation in wiggle period (±seconds) */
#define wigglePeriodVariation  3

/** @brief Size of circular buffer for averaging ADC samples */
#define SizeOfAnalogArray 100

/** @brief Number of historical averages tracked for rate-of-change detection */
#define HISTORY_SIZE        5

// Global Variables - ADC Sampling
/** @brief Circular buffer storing raw ADC readings from photocell */
unsigned int AnalogArray[SizeOfAnalogArray];

/** @brief Current write position in the AnalogArray circular buffer */
unsigned int AnalogArrayPosition = 0;

// Global Variables - Light Detection State
/** @brief Current light status (true = light detected, false = dark) */
bool        lightStatus = true;

/** @brief Previous sum of ADC values, used for change detection */
signed long priorSum;

/** @brief Circular buffer of recent sum values for rate-of-change calculation */
signed long sumHistory[HISTORY_SIZE];

/** @brief Current write position in the sumHistory circular buffer */
unsigned int historyPosition = 0;

/** @brief Flag indicating if sumHistory buffer has been filled at least once */
bool         historyFilled = false;

// Global Variables - Timing
/** @brief Counter tracking seconds for wiggle timing */
int         wiggleSec = 0;

/** @brief Next wiggle time with random variation to evade detection */
int         nextWiggleTime = wigglePeriod;

/**
 * @brief Arduino setup function - initializes hardware and starts timers
 * @details Configures GPIO pins for photocell voltage divider, initializes
 *          sampling buffers, and starts interrupt-driven data collection.
 */
void setup()
{
  // Configure photocell voltage divider pins
  digitalWrite(photoCellGND, LOW);
  pinMode(photoCellGND, OUTPUT);

  pinMode(photoCellSense, INPUT);

  digitalWrite(photoCellVCC, HIGH);
  pinMode(photoCellVCC, OUTPUT);

  // Configure LED indicator
  digitalWrite(led_pin, LOW);
  pinMode(led_pin, OUTPUT);

  // Pre-fill analog sample buffer to ensure stable initial average
  for (int counter = 0 ; counter < SizeOfAnalogArray ; counter++)
    AnalogArray[counter++] = analogRead(photoCellSense);

  // Calculate initial sum and populate history buffer
  signed long initialSum = 0;
  for (int counter = 0 ; counter < SizeOfAnalogArray ; counter++)
    initialSum += (signed long) AnalogArray[counter];

  for (int i = 0; i < HISTORY_SIZE; i++)
    sumHistory[i] = initialSum;

  priorSum = initialSum;

  // Initialize random seed using ADC noise from unconnected pin
  randomSeed(analogRead(A2) + analogRead(A3) + millis());

  // Setup Timer1 for ADC sampling at 100 Hz (100 samples per second)
  Timer1.initialize((1 * 1000000 /*1uS*/) / SizeOfAnalogArray);
  Timer1.attachInterrupt(readAnalogIntoArray);

  // Setup Timer3 for 1-second interrupt (wiggle timing)
  // Note: max timer interval is 8388480 or 8.3s
  // See https://playground.arduino.cc/Code/Timer1/
  Timer3.initialize(1000000 /*1uS*/);
  Timer3.attachInterrupt(sendWiggle);

  // Initialize serial communication for debugging
  Serial.begin(115200);
  // Uncomment below to wait for serial connection before continuing
  //   while (!Serial) {
  //    ; // wait for serial port to connect. Needed for native USB port only
  //  }

  // Initialize USB HID mouse interface
  Mouse.begin();
}

/**
 * @brief Timer1 ISR - reads ADC values and performs light change detection
 * @details Called by Timer1 interrupt at 100 Hz to continuously sample photocell.
 *          When buffer fills (every 1 second), analyzes light changes using
 *          dual-criteria algorithm to distinguish sudden vs gradual changes.
 *
 * Algorithm:
 * - Maintains circular buffer of ADC readings for noise averaging
 * - Calculates percentage change from previous reading
 * - Calculates rate of change over HISTORY_SIZE samples
 * - Triggers light state change only if BOTH criteria met:
 *   1. Magnitude: |percentChange| > SUDDEN_CHANGE_THRESHOLD
 *   2. Rate: |rateOfChange| > CHANGE_RATE_THRESHOLD
 *
 * This dual-criteria approach filters out:
 * - Gradual changes: sunrise, sunset, cloud movement (high magnitude, low rate)
 * - Noise: random fluctuations (low magnitude)
 *
 * While responding to:
 * - Light switch on/off (high magnitude, high rate)
 */
void readAnalogIntoArray(void)
{
  // Sample photocell and advance circular buffer position
  AnalogArray[AnalogArrayPosition++] = analogRead(photoCellSense);

  // Wrap buffer position and check if complete cycle finished
  AnalogArrayPosition = AnalogArrayPosition % SizeOfAnalogArray;

  // Process detection algorithm when buffer wraps (once per second)
  if (!AnalogArrayPosition)
  {
    String toSerial = "";

    // Calculate sum of all samples for averaging
    signed long sum = 0;
    for (int counter = 0 ; counter < SizeOfAnalogArray ; counter++)
      sum += (signed long) AnalogArray[counter];

    // Compute immediate percentage change from previous measurement
    signed long diff = sum - priorSum;
    signed long percentChange = (diff * 100) / priorSum;

    // Compute rate of change over history window
    // This distinguishes sudden changes (light switch) from gradual (sunset/sunrise)
    signed long oldestSum = sumHistory[historyPosition];
    signed long totalDiff = sum - oldestSum;
    signed long rateOfChange = (totalDiff * 100) / (oldestSum * HISTORY_SIZE);

    // Update history circular buffer with current sum
    sumHistory[historyPosition] = sum;
    historyPosition = (historyPosition + 1) % HISTORY_SIZE;
    if (historyPosition == 0)
      historyFilled = true;

    // Build diagnostic output string
    toSerial += "("; toSerial += millis(); toSerial += "ms) ";
    toSerial += "ADC Prior: "; toSerial += priorSum;
    toSerial += "\tADC Current: "; toSerial += sum;
    toSerial += "\tDiff: "; toSerial += diff;
    toSerial += "\tChange%: "; toSerial += percentChange;
    toSerial += "\tRate%: "; toSerial += rateOfChange;

    // Store current sum for next iteration
    priorSum = sum;

    if (Serial)
      Serial.println(toSerial);

    // Only process changes after history buffer filled (ensures valid rate calculation)
    if (historyFilled) {
      // Detect sudden light increase (light turned ON)
      // Requires BOTH significant magnitude AND high rate of change
      if (percentChange > SUDDEN_CHANGE_THRESHOLD && abs(rateOfChange) > CHANGE_RATE_THRESHOLD) {
        if (Serial)
          Serial.println("Lights Detected ON (sudden change)");
        lightStatus = true;
      }
      // Detect sudden light decrease (light turned OFF)
      // Requires BOTH significant magnitude AND high rate of change
      else if (percentChange < -SUDDEN_CHANGE_THRESHOLD && abs(rateOfChange) > CHANGE_RATE_THRESHOLD) {
        if (Serial)
          Serial.println("Lights Detected OFF (sudden change)");
        lightStatus = false;
      }
      // Log gradual changes that are intentionally ignored
      // (sunrise, sunset, clouds - high magnitude but low rate)
      else if (abs(percentChange) > 5 && abs(rateOfChange) <= CHANGE_RATE_THRESHOLD) {
        if (Serial) {
          toSerial = "Gradual change ignored (";
          toSerial += percentChange;
          toSerial += "% change, ";
          toSerial += rateOfChange;
          toSerial += "% rate)";
          Serial.println(toSerial);
        }
      }
    }
  }
}

/**
 * @brief Timer3 ISR - sends mouse wiggle movements when light is detected
 * @details Called by Timer3 interrupt every second. Sends mouse movement
 *          at randomized intervals when lightStatus indicates light is present.
 *          Uses random timing and direction variations to evade detection by
 *          system monitoring software. Movements are minimal (+1/-1) to prevent
 *          screen saver activation without being noticeable to the user.
 */
void sendWiggle(void)
{
  wiggleSec++;

  // Check if randomized wiggle time has been reached
  if (wiggleSec >= nextWiggleTime) {
    wiggleSec = 0; // Reset counter

    // Calculate next wiggle time with random variation
    // Range: (wigglePeriod - wigglePeriodVariation) to (wigglePeriod + wigglePeriodVariation)
    nextWiggleTime = wigglePeriod + random(-wigglePeriodVariation, wigglePeriodVariation + 1);

    // Ensure minimum period of 3 seconds
    if (nextWiggleTime < 3)
      nextWiggleTime = 3;

    if (lightStatus) {
      // Light detected - send wiggle to prevent screen saver
      if (Serial) {
        Serial.print("Need to wiggle the mouse (next in ");
        Serial.print(nextWiggleTime);
        Serial.println("s)");
      }

      // Randomize movement direction to evade pattern detection
      // Randomly choose one of four diagonal directions
      int moveX = (random(2) == 0) ? 1 : -1;
      int moveY = (random(2) == 0) ? 1 : -1;

      // Send minimal movement in random direction
      Mouse.move(moveX, moveY, 0);
      delay(1);
      // Return to original position
      Mouse.move(-moveX, -moveY, 0);

      // Turn on LED indicator
      digitalWrite(led_pin, HIGH);
    }
    else {
      // Dark - do not wiggle, allow screen saver
      if (Serial)
        Serial.println("Do not wiggle the mouse");

      // Turn off LED indicator
      digitalWrite(led_pin, LOW);
    }
  }
}

/**
 * @brief Arduino main loop - empty as all work done in timer interrupts
 * @details All functionality is handled by Timer1 and Timer3 ISRs:
 *          - Timer1: Photocell sampling and light detection
 *          - Timer3: Mouse wiggle generation
 */
void loop()
{
  // Intentionally empty - interrupt-driven architecture
}
