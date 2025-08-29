// TODOS
// 1. Implement buffering in web server to improve download speed
// 2. Implement data recording in 2 second chunks
// 3. Implement sleeping in 2 second chunks



#include <OneWire.h> //used to read temperature sensor
#include <DallasTemperature.h> //used to read temperature sensor
#include <WiFiS3.h> //used to host web server for data
#include <SD.h> //used to communicate with SD card
#include <SPI.h> //needed to communicate with SD card? (not certain it's necessary)

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
unsigned long cycleStart = 0; // start of 10 minute measure/sleep cycle
const unsigned long measureCycle = 600000; // 10 minute combined measure/sleep cycle length
const unsigned long apTimeout = 600000; // 10 minute timeout
File sdFile;
const char* const csvHeaders[] = {
  "time (millis)",
  "Temperature Median",
  //"Temperature Average",
  //"Temperature Minimum",
  //"Temperature Maximum",
  //"Temperature StdDev",
  "LED ON Turbidity Median",
  "LED OFF Turbidity Median",
  "Turbidity Median",
  //"Turbidity Average",
  //"Turbidity Minimum",
  //"Turbidity Maximum",
  //"Turbidity StdDev",
  "TDS Median",
  //"TDS Average",
  //"TDS Minimum",
  //"TDS Maximum",
  //"TDS StdDev",
  "Pressure Median (MPa)",
  //"Pressure Average (MPa)",
  //"Pressure Minimum (MPa)",
  //"Pressure Maximum (MPa)",
  //"Pressure StdDev (MPa)"
  //"Battery Level (%)" //t Add at a later time
};

// Globals used for reading sequence
int temperatureBuffer[30];
int LEDOnBuffer[30];
int LEDOffBuffer[30];
int turbidityDiffBuffer[30];
int TDSBuffer[30]; 

// NOTE: WEB SERVER CAN BE ACCESSED ONLY AT: http://192.168.4.1
const char ssid[] = "EMUWaterQuality"; // name of wifi Access Point/hotspot
const char pass[] = ""; //password to wifi Access Point: "" results in no password requirement
WiFiServer server(80); //web server port set to 80 (standard for http)

/*
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
*/


/////////////// Setup ////////////////////////////////////////////////////
void setup() {
  pinMode(buttonPin, INPUT_PULLUP); // pin tied to ground when button latched
  pinMode(emitPin, OUTPUT); // set up emitPin to power LED
  Serial.begin(9600); // initialize serial communication
  sensors.begin(); // initialize oneWire bus, prepares temp sensor for communication
  buttonState = digitalRead(buttonPin); //read button PIN state upon startup
  delay(2000); // For some reason waiting 2 seconds seems to help give time to detect the SD Card
  if (!SD.begin()){
    Serial.println("SD Card initialization failed!");
  }
  Serial.println("SD card initialized.");

  //kick off the measure/sleep cycle the first time the monitor kicks on
  cycleStart = millis(); 
  readingActive = true;

}
///////////// Loop ///////////////////////////////////////////////////////
void loop() {
  bool buttonState = digitalRead(buttonPin);
  
  // Detect State Change of the Latching Button
  if (buttonState != lastButtonState){
    lastButtonState = buttonState;
    delay(50); //debounce

    // button state has been changed and AP should go from inactive to active
    if (!apActive){
      // if AP is turned on mid reading, throw away partial data and turn the read off
      if (readingActive){
        clearReadingBuffer();
      }
      startAccessPoint();
    } else {
      stopAccessPoint();
    }
  }
  // if AP is active now, then the reading logic is not
  if (apActive){

    // PUT ALL THE WEB SERVER BEHAVIOR HERE
    WiFiClient client = server.available();
    if (!client) return;
    // Wait for client request
    while (client.connected() && !client.available()) delay(1);

    String request = client.readStringUntil('\r');
    client.flush();

    Serial.println(request);
      // Root page -> show file list
    if (request.indexOf("GET / ") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();

      client.println("<!DOCTYPE HTML>");
      client.println("<html><body>");
      client.println("<h2>SD Card File List</h2>");
      client.println("<ul>");

      File root = SD.open("/");
      listFilesHTML(root, client);

      client.println("</ul>");
      client.println("</body></html>");
      root.close();
    } 
    // File download request
    else if (request.indexOf("GET /download?file=") >= 0) {
      int start = request.indexOf("file=") + 5;
      int end = request.indexOf(" ", start);
      String filename = request.substring(start, end);

      Serial.print("Client requested: ");
      Serial.println(filename);

      if (SD.exists(filename)) {
        File downloadFile = SD.open(filename, FILE_READ);

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/octet-stream");
        client.print("Content-Disposition: attachment; filename=");
        client.println(filename);
        client.println("Connection: close");
        client.println();

        // Send file contents
        while (downloadFile.available()) {
          client.write(downloadFile.read());
        }
        downloadFile.close();
      } else {
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: text/html");
        client.println();
        client.println("<h3>File Not Found</h3>");
      }
    }


  } else{
    // If we've made it here, we should either be recording data or sleeping
    if (readingActive){
      // PUT LOGIC TO READ IN 2 SECOND Increments HERE

    } else{
      // Reading is not active, sleep/delay in 2 second increments until it is time to read again
      // (We sleep/delay in 2 second increments so that AP can still wake up quickly if button pressed)

      // PUT LOGIC TO SLEEP IN 2 SECOND Increments HERE
    }

  }











  sensors.requestTemperatures();
  float tempF = sensors.getTempFByIndex(0);
  //Serial.println(tempF);
  int rawTurb = analogRead(turbPin);
  //Serial.println(rawTurb);
  int rawTDS = analogRead(tdsPin);
  //Serial.println(rawTDS);
  int rawPres = analogRead(presPin);
  
 
}

/////////////// Helper Functions/////////////////////////////////////////

void clearReadingBuffer() {
  //clears all buffers used in sensor read sequence and sets readingActive to false
  
  
  readingActive = false;
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
  apTouchWatch = millis();
  apActive = true;
  //digitalWrite(ledPin, HIGH); // show AP active
  Serial.println("Access Point Active!");
}

void stopAccessPoint() {
  //stops the access point
  WiFi.end();
  Serial.println("Access Point Stopped");
  apActive = false;
}

void listFilesHTML(File dir, WiFiClient &client) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      client.print("<li><a href=\"/download?file=");
      client.print(entry.name());
      client.print("\">");
      client.print(entry.name());
      client.println("</a></li>");
    }
    entry.close();
  }
}

void writeValuesToCSV(float values[sizeof(csvHeaders)]) {
  const char* filename = fileNameFromTime();
  // Try to open the file in read mode to check if it exists
  bool fileExists = SD.exists(filename);
  File dataFile = SD.open(filename, FILE_WRITE);
  if (!dataFile) {
    Serial.println("Error opening file!");
    return;
  }
  // If the file didn’t exist, add headers
  if (!fileExists) {
    for (int i = 0; i < sizeof(csvHeaders); i++) {
      dataFile.print(csvHeaders[i]);
      if (i < sizeof(csvHeaders)-1) dataFile.print(",");  // commas between columns
    }
    dataFile.println();  // new line after headers
  }
  // Write the values
  for (int i = 0; i < sizeof(csvHeaders); i++) {
    dataFile.print(values[i], 4); // 4 decimal places
    if (i < 9) dataFile.print(",");
  }
  dataFile.println();
  dataFile.close();
  Serial.println("Values written to CSV.");
}

const char* fileNameFromTime() {
  // Generates file Name based on Current Time, for now each reading will be its own file 
  // which is alright, in the future it will be generated based on date
  static char filename[50]; // persistent buffer across calls
  int uptimeMinutes = millis() / 60000;
  // start with prefix
  strcpy(filename, "SensorReading_Uptime_");
  // append number
  char numBuf[10];
  itoa(uptimeMinutes, numBuf, 10);
  strcat(filename, numBuf);
  // add extension if you want
  strcat(filename, ".csv");
  return filename;
}

float turbRaw2Cal(float raw){
  // use calibration curve to calculate calibrated reading
  return raw; // for now just pass raw value through
}

float tdsRaw2Cal(float raw){
  // use calibration curve to calculate calibrated reading
  return raw; // for now just pass raw value through
}

float presRaw2Cal(float raw){
  // according to specs outputs 0.5-4.5V
  // linearly scales between 0 MPa and 1.6 MPa
  // y = mx + b where x is raw value, m = 0.4 MPa, and b = -0.2 MPa
  return ((raw * 0.4) - 0.2);
}

float analogRead2Volts(int analogValue) {
  // Converts raw ADC reading to voltage
  // Uno R4 WiFi has a 12-bit ADC (0-4095)
  const float referenceVoltage = 5.0; 
  const int adcResolution = 4095; 
  return (analogValue * referenceVoltage) / adcResolution;
}
