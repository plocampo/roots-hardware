/* MultiSensorServoSendCode.ino
 * 2 May 2018
 * 
 * Sixth draft of hardware code
 * Includes multiple sensors and servo, lacking WiFi integration
 * Numerical values subject to change, but general mechanism is functional
 * Sends status to server upon state change
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
#include "WiFiEsp.h"

/* Constants */
/* Arduino-specific */
const int REST_ID = 1234;
const int TABLE_ID = 69;

/* Pins for the servo motor (and some servo-motor-specific constants): */
const int SERVO_PIN = 9;                        // (Chosen b/c Servo.h autoreserves this pin and pin 10 for the servo)

const int POSITIONS[] = {15, 30, 15, 0};        // In degrees; list of "positions" for the servo motor (first item in list is default pos)
const int TURN_MILLI = 15;                      // In milliseconds; time needed for the servo motor to change positions

/* Pins for the ultrasonic sensor (and some sensor-specific constants): */
const int ECHO_PINS[] = {13, 11, 6, 4};         // List of "output" pins of all ultrasonic sensors
const int TRIG_PINS[] = {12, 8, 5, 3};          // Trigger/switch/input pin for ultrasonic sensor

const float CLEAR_MICRO = 2;                    // In microseconds; time needed to clear pin before resending sound pulse
const float TRIGGER_MICRO = 10;                 // In microseconds; time needed for trigger pin to be on HIGH to get accurate readings

/* Constants for WiFi communication */
const char NETWORK_SSID[] = "Virus Detected";   // Network SSID the Arduino should connect to
const char NETWORK_PASS[] = "magbayadka";       // Password of above network

const char SERVER_URL[] = "192.168.8.102";         // URL/ip address of server/website to connect to
const int SERVER_PORT = 8000;                   // Port of server to connect to

/* Other constants: */
// Physical constants
const int SCAN_MILLI = 0.5 * 1000;              // In milliseconds; time interval between Arduino's checks for table objects in one position
const int WAIT_SECS = 2;                        // In seconds; time between Arduino starting a full cycle + send status of checks

const float SOUND_SPEED = 340.0;                // In m/s
const float TABLE_RAD = 1.0 / 2;                // In m; radius of table (assume that all sides are equidistant,
                                                //  and that sensor is placed at middle of table)
// Pins for Arduino <-> shield communication
const int RX_PIN = 2;
const int TX_PIN = 3;

/* Object instantiations */
Servo sweepServo;                 // Instantiation of servo object

/* Global variables (to be saved between loops) */
int isTaken;                        // Current sensed status of table (is it taken or not?)
int lastPos;                        // Position to rotate from

int wiFiStatus = WL_IDLE_STATUS;    // status of the WiFi radio

/* len() hash define because I've been spoiled by Python */
#define len(arr) (sizeof(arr) / sizeof(arr[0]))

// Emulate Serial communication (for Arduino <-> shield)
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(RX_PIN, TX_PIN);
#endif

void setup() {
  /* Initialize pins */
  // Attach servo to pin 9
  sweepServo.attach(9);

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

  /* Initialize serial communication(s) */
  // Start serial communication (for debugging console, for now)
  // (Note that the Serial Monitor baud rate will need to be set to 115200)
  Serial.begin(115200);
  // Initialize serial used by the ESP module
  Serial1.begin(9600);
  // Initialize ESP module
  WiFi.init(&Serial1);

  /* Connect to the WiFi network */
  while (wiFiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.print(NETWORK_SSID);
    // Attempt to connect to network
    wiFiStatus = WiFi.begin(NETWORK_SSID, NETWORK_PASS);
  }

  // If reached here, are now connected to the network
  Serial.println("You are connected to the network: ");
  printWiFiStatus();
}

void loop() {  
  delay(WAIT_SECS * 1000);
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
    /* Send updated status to server*/
    Serial.println("--> " + formatData(REST_ID, TABLE_ID, isTaken));
    postData(SERVER_URL, SERVER_PORT, formatData(REST_ID, TABLE_ID, isTaken));
  } else if (isTaken && !isFound) {
    isTaken = false;
    Serial.println("Table is vacant.");
    /* Send updated status to server*/
    Serial.println("--> " + formatData(REST_ID, TABLE_ID, isTaken));
    postData(SERVER_URL, SERVER_PORT, formatData(REST_ID, TABLE_ID, isTaken));
  }
}

/* --------- User-defined functions --------- */

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("IP addr of shield: ");
  Serial.println(WiFi.localIP());
}

String formatData(int restID, int tableID, int takenStatus) {
  String formattedString = "";

  formattedString += "restaurantID=" + String(restID);
  formattedString += "&";
  formattedString += "tableID=" + String(tableID);
  formattedString += "&";
  formattedString += "isTaken=" + String(takenStatus);

  return formattedString;
}

/* HTTP POSTs the given string to the server (as is). */
void postData(char *server_url, int server_port, String dataString) {
  /* Some postData-specific constants.*/
  const int MAX_ATTEMPTS = 3;      // The number of attempts to connect to server before giving up.
  const int RETRY_SECS = 5;        // How long to wait before retrying connecting to the server..
  const int TIMEOUT_SECS = 1;      // How long to wait for an HTTP OK / reply.

  // Initialize Ethernet client object
  WiFiEspClient client;

  int attempts = 0;

  /* Attempt to connect to server. */
  while(attempts < MAX_ATTEMPTS) {
    Serial.println("[postData] Connect attempt #" + String(attempts));
    if (client.connect(server_url, server_port)) {
      Serial.println("[postData] Connected to server!");

      /* Send the HTTP POST. */
      String postString = getPostString("/", String(server_url) + ":" + String(server_port), dataString);
      client.print(postString);

      Serial.println("[postData] Sent my message to server.");
      break;
    } else {
      Serial.println("[postData] Attempt #" + String(attempts) + "failed.");
      attempts++;
      // Wait a second for server to ready before attempting another connection
      delay(RETRY_SECS * 1000);
    }
  }

  if (attempts >= MAX_ATTEMPTS) {
    Serial.println("[postData] Server connection failed! Did not send any data.");
  } else {
    /* Wait for the server disconnecting, at least until some timeout. */
    unsigned long startTime = millis();
    while (millis() - startTime < (TIMEOUT_SECS * 1000)) {
      // Read any reply from the server and print it to terminal
      while(client.available() > 0) {
        char c = client.read();
        Serial.write(c);
      }

      // If server's disconnected, don't bother waiting for timeout
      if (!client.connected()) {
        Serial.println("[postData] The server disconnected.");
        break;       
      }
    }
  }

  /* Stop the client before returning*/
  Serial.println("[postData] Reached end, stopping client.");
  client.stop();

  return;
}

/*  getPostString: Puts POST data into a correctly formatted HTTP POST request.
 *    args:
 *      location - the folder to post to (ex. "/", "/bin")
 *      host - the location to send the string (format: either url or ip:port)
 *      data - the *string*, *verbatim*, to post
 *    returns:
 *      a string containing the corresponding correct HTTP POST request
 */
String getPostString(String location, String host, String data) {
  String postString = "";

  postString += "POST " + location + " HTTP/1.1\r\n";
  postString += "Host: " + host + "\r\n";
  postString += "Connection: close\r\n";
  postString += "Content-Length: " + String(data.length()) + "\r\n";
  postString += "Content-Type: application/x-www-form-urlencoded\r\n";
  postString += "\r\n";
  postString += data;

  return postString;
}
