#include <WiFiS3.h>

const char* ssid = "R4-AccessPoint";
const char* password = "mypassword";
WiFiServer server(80);
int ledPin = 13;

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(9600);

  // Start Wi-Fi in Access Point mode
  WiFi.beginAP(ssid, password);
  Serial.print("Starting AP: ");
  Serial.println(ssid);

  Serial.println("\nAccess Point started!");
  IPAddress IP = WiFi.localIP();
  Serial.print("Connect to http://");
  Serial.println(IP);

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");

    String request = client.readStringUntil('\r');
    client.flush();

    Serial.print("Request: ");
    Serial.println(request);

    // Blink on command
    if (request.indexOf("GET /blink") >= 0) {
      digitalWrite(ledPin, HIGH);
      delay(500);
      digitalWrite(ledPin, LOW);
    }

    // Send HTML response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE html><html>");
    client.println("<head><title>LED Blink</title></head>");
    client.println("<body>");
    client.println("<h1>Arduino Uno R4 WiFi</h1>");
    client.println("<p><a href=\"/blink\"><button>Blink LED</button></a></p>");
    client.println("</body></html>");

    delay(1);
    client.stop();
    Serial.println("Client disconnected");
  }
}

