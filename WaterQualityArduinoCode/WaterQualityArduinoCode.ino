#include <OneWire.h> //used to read temperature sensor
#include <DallasTemperature.h> //used to read temperature sensor
#include <WiFiS3.h> //used to host web server for data
#include <SD.h> //used to communicate with SD card

///////////////////// Pin Definitions ////////////////////////
// Latching Wifi Wake button is on pin 2
int buttonPin = 2;

// digital pin 4 has temperature sensor
int tempPin = 4;
#define ONE_WIRE_BUS tempPin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// SD Card wiring configuration uses chipSelect 10
int chipSelect = 10;

// A0 has turbidity meter
int turbPin = A0;

// A1 has pressure sensor
int presPin = A1;

// A2 has TDS meter (total dissolved solids)
int tdsPin = A2;

// Turbidity emitter is powered by pin 3
int emitPin = A3;

////////////////////// Global Variables ///////////////////////

bool apActive = false; // Access point (hotspot) running?
bool lastButtonState = LOW; //initialize button state
bool readingActive = false; // show whether sensor read sequence is active
unsigned long apTouchWatch = 0; // timestamp of last time the user interacted with the access point
const unsigned long apTimeout = 600000 // 10 minute timeout
// NOTE: WEB SERVER CAN BE ACCESSED ONLY AT: http://192.168.4.1
const char ssid[] = "EMUWaterQuality"; // name of wifi Access Point/hotspot
const char pass[] = ""; //password to wifi Access Point: "" results in no password requirement
WiFiServer server(80); //web server port set to 80 (standard for http)

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


void setup() {
  pinMode(buttonPin, INPUT_PULLUP); //pin tied to ground when button latched
  pinMode(emitPin, OUTPUT); //set up emitPin to power LED
  Serial.begin(9600); //initialize serial communication
  sensors.begin(); //initialize oneWire bus, prepares temp sensor for communication

}

void loop() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempFByIndex(0);
  Serial.println(tempC);
  int rawTurb = analogRead(turbPin);
  //Serial.println(rawTurb);
  int rawTDS = analogRead(tdsPin);
  //Serial.println(rawTDS);
  delay(250);
 
}
