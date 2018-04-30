/* MultiSensorServoCode.ino
 * 1 May 2018
 * 
 * Fifth draft of hardware code
 * Includes multiple sensors and servo, lacking WiFi integration
 * Numerical values subject to change, but general mechanism is functional
 * 
 * Servo configuration: changes position by 15 degrees at a time, ranging from 0 to 30
 * 
 * See https://howtomechatronics.com/tutorials/arduino/ultrasonic-sensor-hc-sr04/ for
 * the explanation on how to use the ultrasonic sensor.
 * The code below makes use of the Servo library, which can be downloaded here:
 * https://github.com/arduino-libraries/Servo
 */

/* Libraries */
#include <Servo.h>

/* Constants */

/* Pins for the servo motor (and some servo-motor-specific constants): */
const int SERVO_PIN = 9;            // (Chosen b/c Servo.h autoreserves this pin and pin 10 for the servo)

const int POSITIONS[] = {15, 30, 15, 0};        // In degrees; list of "positions" for the servo motor (first item in list is default pos)
const int TURN_MILLI = 15;          // In milliseconds; time needed for the servo motor to change positions

/* Pins for the ultrasonic sensor (and some sensor-specific constants): */
const int ECHO_PINS[] = {13, 11, 6, 4};       // List of "output" pins of all ultrasonic sensors
const int TRIG_PINS[] = {12, 8, 5, 3};            // Trigger/switch/input pin for ultrasonic sensor

const float CLEAR_MICRO = 2;        // In microseconds; time needed to clear pin before resending sound pulse
const float TRIGGER_MICRO = 10;     // In microseconds; time needed for trigger pin to be on HIGH to get accurate readings

/* Other constants: */
const int SCAN_MILLI = 0.5 * 1000;    // In milliseconds; time interval between Arduino's checks for table objects

const float SOUND_SPEED = 340.0;    // In m/s
const float TABLE_RAD = 1.0 / 2;   // In m; radius of table (assume that all sides are equidistant,
                                    //  and that sensor is placed at middle of table)

/* Object instantiations */
Servo sweepServo;                 // Instantiation of servo object

/* Global variables (to be saved between loops) */
int isTaken;                        // Current sensed status of table (is it taken or not?)
int lastPos;                        // Position to rotate from

/* len() hash define because I've been spoiled by Python */
#define len(arr) (sizeof(arr) / sizeof(arr[0]))

void setup() {
  // Attach servo to pin 9
  sweepServo.attach(9);
  
  // Start serial communication (for debugging console, for now)
  Serial.begin(9600);

  // Setup servo pin
  pinMode(SERVO_PIN, OUTPUT);

  // Setup sensor pins
  for (int i = 0; i < len(TRIG_PINS); i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
  }
  for (int i = 0; i < len(ECHO_PINS); i++) {
    pinMode(ECHO_PINS[i], INPUT);
  }

  // Set default table-taken status on start-up to be taken
  isTaken = true;

  // Initialize saved position to 0
  lastPos = 0;
}

void loop() {  
  /* Should probably add something here to make the Arduino wait SCAN_SECS before checking again. */
  int isFound = false;

  Serial.println("Starting looking around...");
  // For every "rotated" position of the servo motor
  for (int i = 0; i < len(POSITIONS); i++) {    
    int currPos = POSITIONS[i];
    // Rotate in increments to reduce noise
    while (lastPos != currPos) {
      if (lastPos < currPos) {
        lastPos++;
      } else {
        lastPos--;
      }
      sweepServo.write(lastPos);
      delay(TURN_MILLI);
    }
    Serial.println("Turned to new position.");
    // For every sensor attached to the servo motor:
    for (int j = 0; j < len(ECHO_PINS); j++) {
      int currEchoPin = ECHO_PINS[j];
      int currTrigPin = TRIG_PINS[j];
      
      // Clear the trigger pin before taking measurement
      digitalWrite(currTrigPin, LOW);
      delayMicroseconds(CLEAR_MICRO);

      // Send the sound pulse
      digitalWrite(currTrigPin, HIGH);
      delayMicroseconds(TRIGGER_MICRO);
      digitalWrite(currTrigPin, LOW);

      // Wait for the response (returnTime is in microseconds, or 0 if no
      //  complete pulse returned)
      int returnTimeMicro = pulseIn(currEchoPin, HIGH);

      // Compute the distance of the object based on the reply time
      float approxDistMeter = (SOUND_SPEED * (float(returnTimeMicro) / 1000000)) / 2;
      // float approxDistMeter = (returnTimeMicro/2) / 29.1;

      if (returnTimeMicro <= 0 || approxDistMeter <= 0 || approxDistMeter > TABLE_RAD) {
        // Serial.println("Out of range (on this face)!");
        // Serial.println(approxDistMeter);
      } else {
        // Serial.print("Return time (microseconds): ");
        // Serial.println(returnTimeMicro);
        // Serial.print("Distance computed (meters): ");
        // Serial.println(approxDistMeter);

        // Take note that an object was found during the scan cycle
        if (!isFound){
          isFound = true;
        }
      }
    }
    delay(SCAN_MILLI);    // Wait before next turn
  }

  // Update and send new status to server if status has changed
  if (!isTaken && isFound) {
    isTaken = true;
    Serial.println("Table is taken.");
    /* Send updated status to server -- add this later! */
  } else if (isTaken && !isFound) {
    isTaken = false;
    Serial.println("Table is vacant.");
    /* Send updated status to server -- add this later! */
  }
}
