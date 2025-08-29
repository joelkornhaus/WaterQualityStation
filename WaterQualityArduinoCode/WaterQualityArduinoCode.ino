// TODOS
// 1. Implement buffering in web server to improve download speed - this can wait until later
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
// MOSI - 11
// MISO - 12
// SCK - 13

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
bool buttonState; // Initialize current button state
bool lastButtonState; // Initialize button state
bool readingActive; // show whether sensor read sequence is active
unsigned long apTouchWatch = 0; // timestamp of last time the user interacted with the access point
unsigned long readCycleStart; // start of 10 minute measure/sleep cycle
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
int readIndex = 0; // initialize the reading index to be 0
float temperatureBuffer[30];
float LEDOnBuffer[30];
float LEDOffBuffer[30];
float turbidityDiffBuffer[30];
float TDSBuffer[30];
float presBuffer[30];

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
  readCycleStart = 0; 
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
    // If we've made it here, we should either be recording data or sleeping, depending how much time has passed
    // check whether it's time to update ReadingActive flag
    if (millis()-readCycleStart > 10000){
      readingActive = true;
      readCycleStart = millis();
    }

    if (readingActive){
      // PUT LOGIC TO READ IN 2 SECOND Increments HERE
      // first 30 reads get temperature and emitter off value
      if (readIndex < 30) {
        digitalWrite(emitPin,HIGH);

        // Collect Temperature value, no need to calibrate since it's already calibrated. Put it into buffer array
        sensors.requestTemperatures();
        float tempF = sensors.getTempFByIndex(0);
        temperatureBuffer[readIndex] = tempF;
        Serial.println(tempF);
        // Collect Turb value, calibrate it, then put it into buffer array
        int rawTurb = analogRead(turbPin);
        float calTurb = turbRaw2Cal(rawTurb);
        LEDOffBuffer[readIndex] = calTurb;
        Serial.println(calTurb);

        readIndex++;
        delay(2000);

      // next 30 reads get 
      } else if (readIndex > 29 && readIndex < 59 ){
        digitalWrite(emitPin,LOW);

        // Collect TDS Reading, calibrate it, then put it into buffer array
        int rawTDS = analogRead(tdsPin);
        float calTDS = tdsRaw2Cal(rawTDS);
        TDSBuffer[readIndex-30] = calTDS;
        Serial.println(calTDS);
        // Collect Pressure Reading, calibrate it, then put it into buffer array
        int rawPres = analogRead(presPin);
        float calPres = presRaw2Cal(rawPres);
        presBuffer[readIndex-30] = calPres;
        Serial.println(calPres);
        // Collect Turb value, calibrate it, then put it into buffer array
        int rawTurb = analogRead(turbPin);
        float calTurb = turbRaw2Cal(rawTurb);
        LEDOnBuffer[readIndex-30] = calTurb;
        Serial.println(calTurb);
        readIndex++;
        delay(2000);
      
      } else{
          // at this point, we're still "reading" but have taken all of our data, finish the calculations and write then to the CSV
          
          // populate turbidityDiffBuffer by subtracting LEDOff from LEDOn
          subtractArrays(LEDOnBuffer,LEDOffBuffer,turbidityDiffBuffer,sizeof(LEDOnBuffer)/sizeof(LEDOnBuffer[0]));

          float tempMed = median(temperatureBuffer,sizeof(temperatureBuffer)/sizeof(temperatureBuffer[0]));
          float LEDOnMed = median(LEDOnBuffer,sizeof(LEDOnBuffer)/sizeof(LEDOnBuffer[0]));
          float LEDOffMed = median(LEDOffBuffer,sizeof(LEDOffBuffer)/sizeof(LEDOffBuffer[0]));
          float turbMed = median(turbidityDiffBuffer,sizeof(turbidityDiffBuffer)/sizeof(turbidityDiffBuffer[0]));
          float tdsMed = median(TDSBuffer,sizeof(TDSBuffer)/sizeof(TDSBuffer[0]));
          float presMed = median(presBuffer,sizeof(presBuffer)/sizeof(presBuffer[0]));

          // Buffers for converted float values
          char tempStr[12];
          char ledOnStr[12];
          char ledOffStr[12];
          char turbStr[12];
          char tdsStr[12];
          char presStr[12];

          // Convert floats to strings
          dtostrf(tempMed, 1, 4, tempStr);
          dtostrf(LEDOnMed, 1, 4, ledOnStr);
          dtostrf(LEDOffMed, 1, 4, ledOffStr);
          dtostrf(turbMed, 1, 4, turbStr);
          dtostrf(tdsMed, 1, 4, tdsStr);
          dtostrf(presMed, 1, 4, presStr);
          char* time = uptimeString();
          // Put everything (including time string) into char* array
          const char* fields[] = {time, tempStr, ledOnStr, ledOffStr, turbStr, tdsStr, presStr};

          writeValuesToCSV(fields,7);
          //clear Reading Buffers in order to set reading active flag to false (and perform other cleanup actions
          //that may be added in the future)
          clearReadingBuffer();
      }

    } else{
      // Reading is not active, sleep/delay in 2 second increments until it is time to read again
      // (We sleep/delay in 2 second increments so that AP can still wake up quickly if button pressed)
      // PUT LOGIC TO SLEEP IN 2 SECOND Increments HERE
      delay(2000);
    }

  }


  //sensors.requestTemperatures();
  //float tempF = sensors.getTempFByIndex(0);
  //Serial.println(tempF);
  //digitalWrite(emitPin, HIGH);
  //int rawTurb = analogRead(turbPin);
  //Serial.println(rawTurb);
  //int rawTDS = analogRead(tdsPin);
  //Serial.println(rawTDS);
  //int rawPres = analogRead(presPin);
  //float calPres = presRaw2Cal(rawPres);
  //Serial.println(calPres);
 
}

/////////////// Helper Functions/////////////////////////////////////////

void clearReadingBuffer() {
  //clears all buffers used in sensor read sequence and sets readingActive to false
  readIndex = 0; // initialize the reading index to be 0
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

void writeValuesToCSV(const char* fields[], int fieldCount) {
  const char* filename = fileNameFromTime();
  bool fileExists = SD.exists(filename);
  File dataFile = SD.open(filename, FILE_WRITE);

  if (!dataFile) {
    Serial.println("Error opening file!");
    return;
  }

  // Write headers if the file is new
  if (!fileExists) {
    for (int i = 0; i < sizeof(csvHeaders) / sizeof(csvHeaders[0]); i++) {
      dataFile.print(csvHeaders[i]);
      if (i < (sizeof(csvHeaders) / sizeof(csvHeaders[0])) - 1) dataFile.print(",");
    }
    dataFile.println();
  }

  // Write all fields
  for (int i = 0; i < fieldCount; i++) {
    dataFile.print(fields[i]);
    if (i < fieldCount - 1) dataFile.print(",");
  }
  dataFile.println();

  dataFile.close();
  Serial.println("Values written to CSV.");
}


char* fileNameFromTime() {
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

float turbRaw2Cal(int raw){
  // use calibration curve to calculate calibrated reading
  return raw; // for now just pass raw value through
}

float tdsRaw2Cal(int raw){
  // use calibration curve to calculate calibrated reading
  return raw; // for now just pass raw value through
}

float presRaw2Cal(int raw){
  // according to specs outputs 0.5-4.5V
  // linearly scales between 0 MPa and 1.6 MPa
  // y = mx + b where x is raw value, m = 0.4 MPa, and b = -0.2 MPa
  float volts = analogRead2Volts(raw);
  return ((volts * 0.4) - 0.2);
}

float analogRead2Volts(int analogValue) {
  // Converts raw ADC reading to voltage
  // Uno R4 WiFi has a 12-bit ADC (0-4095)
  const float referenceVoltage = 5.0; 
  const int adcResolution = 4095; 
  return (analogValue * referenceVoltage) / adcResolution;
}

float median(float arr[], int size) {
  // Copy input array into a temporary array
  float temp[size];
  for (int i = 0; i < size; i++) {
    temp[i] = arr[i];
  }

  // Simple bubble sort (fine for small arrays like yours)
  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (temp[j] > temp[j + 1]) {
        float t = temp[j];
        temp[j] = temp[j + 1];
        temp[j + 1] = t;
      }
    }
  }
  // Return median
  if (size % 2 == 0) {
    // Even number of elements → average of middle two
    return (temp[size/2 - 1] + temp[size/2]) / 2.0;
  } else {
    // Odd number of elements → middle value
    return temp[size/2];
  }
}

void subtractArrays(const float* a, const float* b, float* result, int size) {
  for (int i = 0; i < size; i++) {
    result[i] = a[i] - b[i];
  }
}

char* uptimeString() {
  unsigned long ms = millis() / 1000; // convert to seconds
  unsigned long seconds = ms % 60;
  unsigned long minutes = (ms / 60) % 60;
  unsigned long hours   = (ms / 3600) % 24;
  unsigned long days    = ms / 86400;

  // static buffer so it persists after function returns
  static char buffer[20];  
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu:%02lu",
           days, hours, minutes, seconds);
  return buffer;
}
