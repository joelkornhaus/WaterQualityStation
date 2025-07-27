#include <OneWire.h>
#include <DallasTemperature.h>

// digital pin 4 has temperature sensor
int tempPin = 4;
#define ONE_WIRE_BUS tempPin


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// A2 has turbidity meter
int turbPin = A0;
// A5 has TDS meter (total dissolved solids
int tdsPin = A2;

  
void setup() {
  // put your setup code here, to run once:
  // All sensor boards have power from same 5V pin and ground to different grounds

  
  //initialize serial communication
  Serial.begin(9600);
  sensors.begin();

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
