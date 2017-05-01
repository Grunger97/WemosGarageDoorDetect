/*
  Basic sketch to detect a button press and send it to io.adafruit.com.

  This is listening for a button press/release on D4 (GPIO 2).
  Once a change in state is detected, it will send an MQTT message to io.adafruit.com
  to record the new value.

  This also has a simple webserver in it to serve up a page that allows the 
  internal LED to be toggled on/off.
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "****"
#define WLAN_PASS       "****"

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "****"
#define AIO_KEY         "****"

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

ESP8266WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish garage1 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/garage1");
Adafruit_MQTT_Publish garage2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/garage2");

// Setup a feed called 'onoff' for subscribing to changes.
//Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");
 
int internalLEDPin;
int internalLEDValue;
int value1;
int value2;

void handleRoot()
{
  String message = "Garage Door Detector!! Use ";
  message += "/upload to update the flash with new code. Use the .bin file created by the IDE. Use /status to see current status.";
  server.send(200, "text/plain", message);
}

void handleUpload()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html", serverIndex);
}

void handleUpdate()
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
  ESP.restart();
}

void handleUpdate2()
{
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    Serial.setDebugOutput(true);
    WiFiUDP::stopAll();
    Serial.printf("Update: %s\n", upload.filename.c_str());
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if(!Update.begin(maxSketchSpace)){//start with max available size
      Update.printError(Serial);
    }
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
      Update.printError(Serial);
    }
  } else if(upload.status == UPLOAD_FILE_END){
    if(Update.end(true)){ //true to set the size to the current progress
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);
  }
  yield();
}

void handleStatus()
{
  for (uint8_t i=0; i<server.args(); i++)
  {
    if (server.argName(i) == "internal")
    {
      if (server.arg(i) == "off")
      {
        internalLEDValue = HIGH;
      }
      else if (server.arg(i) == "on")
      {
        internalLEDValue = LOW;
      }
      digitalWrite(internalLEDPin, internalLEDValue);
    }
  }
  
  String message = "<html>\n<head>";
  message += "<style>\n";
  message += "table {\n";
  message += "  border-collapse : collapse;\n";
  message += "}\n";
  message += "table, th, td {border : 1px solid black;}\n";
  message += "th {background-color : lightblue;}\n";
  message += "td {text-align : center;}\n";
  message += "</style>\n";
  message += "</head><body>";
  message += "LED directory\n\n";
  message += "<table><tr><th>LED</th><th>State</th></tr>";
  message += "<tr><td>Internal</td><td><a href=\"/LED?internal=";
  message += internalLEDValue == HIGH ? "on" : "off";
  message += "\">";
  message += internalLEDValue == HIGH ? "Off" : "On";
  message += "</a></td></tr>";
  message += "<tr><td>Garage Door 1</td><td>";
  message += value1 == LOW ? "Open" : "Closed";
  message += "</td></tr>";
  message += "<tr><td>Garage Door 2</td><td>";
  message += value2 == LOW ? "Open" : "Closed";
  message += "</td></tr>";
  message += "</table>";
  message += "<br>";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  message += "</body>";
  message += "</html>";

  for (uint8_t i=0; i<server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(200, "text/html", message);
}

void handleNotFound() 
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i=0; i<server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);

}

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  delay(10);
  pinMode(D3, INPUT);
  pinMode(D8, INPUT);
  Serial.begin(115200);
  delay(1000);
  internalLEDPin = D4;
  internalLEDValue = LOW;  // This is on
  pinMode(internalLEDPin, OUTPUT);
  digitalWrite(internalLEDPin, internalLEDValue);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);
 
  WiFi.begin(WLAN_SSID, WLAN_PASS);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    internalLEDValue = !internalLEDValue;
    digitalWrite(internalLEDPin, internalLEDValue);
  }
  internalLEDValue = LOW;
  digitalWrite(internalLEDPin, internalLEDValue);
  Serial.println("");
  Serial.println("WiFi connected");
 
  // Start the server
  server.begin();
  Serial.println("Server started");
 
  // Print the IP address
  Serial.print("Use this URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  server.on("/", handleRoot);
  server.on("/inline", []()
  {
    server.send(200, "text/plain", "this works as well");
  });
  server.on("/status", handleStatus);
  server.on("/update", HTTP_POST, handleUpdate, handleUpdate2);
  server.on("/upload", handleUpload);
  
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  value1 = digitalRead(D3);
  value2 = digitalRead(D8);
}

// the loop function runs over and over again forever
void loop() {
  int val = digitalRead(D3);
  if (val != value1) {
    Serial.print("Switch1 = ");
    Serial.println(val);

    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    // function definition further below.
    if (MQTT_connect())
    {
      // Now we can publish stuff!
      Serial.print(F("\nSending garage1 val "));
      Serial.print(val);
      Serial.print("...");
      if (! garage1.publish(val)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK!"));
        value1 = val;
      }
      
      delay(100);
      }
  }

  val = digitalRead(D8);
  if (val != value2) {
    Serial.print("Switch2 = ");
    Serial.println(val);
    value2 = val;

    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    // function definition further below.
    if (MQTT_connect())
    {
      // Now we can publish stuff!
      Serial.print(F("\nSending garage2 val "));
      Serial.print(value2);
      Serial.print("...");
      if (! garage2.publish(value2)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK!"));
      }
      
      delay(100);
    }
  }
  // Check if a client has connected
  server.handleClient();
  delay(2);
  //Serial.println("Client disconnected");
  //Serial.println("");
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care of connecting.
boolean MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return true;
  }

  internalLEDValue = HIGH;  // Turn off the LED
  digitalWrite(internalLEDPin, internalLEDValue);
  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
     // basically die and wait for WDT to reset me
     return false;
    }
  }
  
  internalLEDValue = LOW;  // Turn on the LED
  digitalWrite(internalLEDPin, internalLEDValue);
  Serial.println("MQTT Connected!");
  return true;
}
