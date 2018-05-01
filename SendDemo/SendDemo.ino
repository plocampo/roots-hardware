/* SendDemo.ino
 * 
 * Posts data every certain period of time, under the correct conditions:
 * Counter is sent to the specified server around every 5 seconds, but only if
 * it is a multiple of 2.
 * 
 * Uses the WifiEsp.h library found at https://github.com/bportaluri/WiFiEsp/archive/master.zip.
 * Partially based off of the WebClient.ino example code in the WifiEsp library.
 */

#include "WiFiEsp.h"

// Emulate Serial1 on pins 2 (RX) and 3 (TX)
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(2, 3);
#endif

const int SCAN_SECS = 2;

char ssid[] = "replace-with-ssid";             // network SSID Arduino is connected to
char pass[] = "replace-with-pass";                 // password of above network
int wiFiStatus = WL_IDLE_STATUS;            // status of the WiFi radio

char server_url[] = "10.100.0.8";          // URL of server/website to connect to
int server_port = 8000;                    // Port of server/website to connect to

int counter = 0;

void setup() {
  /* Initialize serial communication(s) */
  // Initialize serial output of the Arduino
  // (note that Serial Monitor baud rate will need to be set to 115200)
  Serial.begin(115200);
  // Initialize serial used by the ESP module
  Serial1.begin(9600);
  // Initialize ESP module
  WiFi.init(&Serial1);

  /* Connect to the WiFi network */
  while (wiFiStatus != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Attempt to connect to network
    wiFiStatus = WiFi.begin(ssid, pass);
  }

  // If reached here, are now connected to the network
  Serial.println("You are connected to the network: ");
  printWiFiStatus();

}

void loop() {
  delay(SCAN_SECS * 1000);

  counter++;
  Serial.println("Counter is: " + String(counter));
  if (counter % 2 == 0) {
    Serial.println("Counter is even!");
    postData(String(counter));
  }
}

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("IP addr of shield: ");
  Serial.println(WiFi.localIP());
}

/* HTTP POSTs the given string to the server (as is). */
void postData(String dataString) {
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
      String formattedString = "a=" + dataString;
      String postString = getPostString("/", String(server_url) + ":" + String(server_port), formattedString);
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

