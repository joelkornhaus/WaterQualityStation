// Includes
#include <Arduino.h>
#include <WiFiS3.h>
#include <SD.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Declare Functions Prior to defining them
void startAccessPoint();
void stopAccessPoint();
void handleClient();
void sendHTML(WiFiClient& client, const char* content = nullptr);
void sendCSV(WiFiClient& client);

// Constants and state
const int buttonPin = 2;
const int chipSelect = 10;
const int ledPin = 13;
bool apActive = false;
bool lastButtonState = LOW;
unsigned long apStartTime = 0;
const unsigned long timeout = 600000; // 10 minutes

const char ssid[] = "TestAP";
const char pass[] = "test";

WiFiServer server(80);

// HTML page
const char html[] = R"rawliteral(
<!DOCTYPE html>
<html>
  <head><title>Water Monitor</title></head>
  <body>
    <h1>Water Quality Monitor</h1>
    <button onclick="fetch('/disconnect')">Disconnect</button>
    <a href="/download">Download File</a>
    <script>
      // Future: add status updates or controls
    </script>
  </body>
</html>
)rawliteral";

File root;

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP); // assumes grounded button when pressed
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);

  if (!SD.begin(chipSelect)) {
    Serial.println("SD init failed.");
    return;
  }

  // Load default state
  lastButtonState = digitalRead(buttonPin);

  root = SD.open("/");
  printDirectory(root,0);

}

void loop() {
  bool currentState = digitalRead(buttonPin);

  // Detect state change on the latching button
  if (currentState != lastButtonState) {
    lastButtonState = currentState;
    delay(50); // debounce

    if (!apActive) {
      startAccessPoint();
    } else {
      stopAccessPoint();
    }
  }

  if (apActive) {
    handleClient();
    if (millis() - apStartTime > timeout) {
      stopAccessPoint();
    }
  }

  delay(100); // adjust as needed
}


void startAccessPoint() {
  //attempt to start access point
  //TODO: Investigate why this doesn't work when pw included
  bool result = WiFi.beginAP(ssid); 
  //check for errors
  delay(500);
  if (!result) {
    Serial.println("Failed to start AP!");
  } else {
    Serial.println("AP started");
  }
  IPAddress ip = WiFi.localIP();
  Serial.print("AP IP: "); Serial.println(ip);
  server.begin();
  apStartTime = millis();
  apActive = true;
  //digitalWrite(ledPin, HIGH); // show AP active
  Serial.println("Access Point Active!");
}

void stopAccessPoint() {
  WiFi.end();
  Serial.println("Access Point Stopped");
  apActive = false;
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  String request = client.readStringUntil('\r');
  client.read(); // flush newline

  if (request.indexOf("GET / ") >= 0) {
    sendHTML(client);
  } else if (request.indexOf("GET /download") >= 0) {
    sendCSV(client);
  } else if (request.indexOf("GET /disconnect") >= 0) {
    sendHTML(client, "<h1>Disconnecting...</h1>");
    delay(1000);
    stopAccessPoint();
  }

  client.stop();
}

void sendHTML(WiFiClient& client, const char* content) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  if (content) {
    client.println(content);
  } else {
    client.println(html);
  }
}

void sendCSV(WiFiClient& client) {
  File file = SD.open("data.csv");
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println(); return;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/csv");
  client.println("Content-Disposition: attachment; filename=data.csv");
  client.println("Connection: close");
  client.println();

  while (file.available()) {
    client.write(file.read());
  }
  file.close();
}

