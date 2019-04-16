/*
 Thermocouple Client
*
 This sketch connects takes readings from a thermocouple amplifier,
 batches them and periodically POSTS them to a waiting service using
 a WiFiClient
 */

#include <SPI.h>
#include <WiFi101.h>
#include <Adafruit_MAX31856.h>
#include <ArduinoJson.h>

const char ssid[] = "Brian Spillane's Network";
const char pass[] = "";
const String user = "";
const String unit = "";
const int readingInterval = 30000;
const bool enableLogging = false;

int status = WL_IDLE_STATUS;
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(192,168,0,27);  // numeric IP for Google (no DNS)
//int serverPort = 9000;

const char hostString[] = "Host: zsz5ychlqi.execute-api.us-east-1.amazonaws.com";
const String resetRequest = "POST /prod/" + user + "/" + unit + "/reset HTTP/1.1";
const String sendReadingRequest = "POST /prod/" + user + "/" + unit + "/readings/create HTTP/1.1";
const char connectionClose[] = "Connection: close";

char server[] = "zsz5ychlqi.execute-api.us-east-1.amazonaws.com";
int serverPort = 443;

float thermocoupleTemp = 0;
float coldJunctionTemp = 0;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiSSLClient client;

// Initialize the thermocouple amplifier
Adafruit_MAX31856 max = Adafruit_MAX31856(13, 12, 11, 10);

void setup() {
  establishSerialConnection();
  connectToWifi();
  sendReset();
  while(client.connected()) {
    logln("client is connected - resetting");
    readClientData();
    delay(1000);
  }
  startThermocouple();
}

void loop() {
  readThermoCouple();
  publishReadings();
  while(client.connected()) {
    logln("client is connected - publishing");
    readClientData();
    delay(1000);
  }
  delay(readingInterval);
}

void establishSerialConnection() {
  if(enableLogging) {
    //Initialize serial and wait for port to open:
    Serial.begin(115200);
    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }  
  }
}

void connectToWifi() {
  if (WiFi.status() == WL_NO_SHIELD) {
    logln("WiFi shield not present");
    while (true); // don't continue:
  }
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    log("Attempting to connect to SSID: ");
    logln(ssid);
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  logln("Connected to wifi");
  printWifiStatus();
}

void startThermocouple() {
  max.begin();
  max.setThermocoupleType(MAX31856_TCTYPE_K);
}

void readThermoCouple() {
  // Check and print any faults
  uint8_t fault = max.readFault();
  if (fault) {
    if (fault & MAX31856_FAULT_CJRANGE) logln("Cold Junction Range Fault");
    if (fault & MAX31856_FAULT_TCRANGE) logln("Thermocouple Range Fault");
    if (fault & MAX31856_FAULT_CJHIGH)  logln("Cold Junction High Fault");
    if (fault & MAX31856_FAULT_CJLOW)   logln("Cold Junction Low Fault");
    if (fault & MAX31856_FAULT_TCHIGH)  logln("Thermocouple High Fault");
    if (fault & MAX31856_FAULT_TCLOW)   logln("Thermocouple Low Fault");
    if (fault & MAX31856_FAULT_OVUV)    logln("Over/Under Voltage Fault");
    if (fault & MAX31856_FAULT_OPEN)    logln("Thermocouple Open Fault");
  }

  logln("---Taking Reading---");
  thermocoupleTemp = max.readThermocoupleTemperature();
  log("Thermocouple Temp: "); logln(thermocoupleTemp);
  coldJunctionTemp = max.readCJTemperature();
  log("Cold Junction Temp: "); logln(coldJunctionTemp);
}

void publishReadings() {
  client.stop();
  logln("\nStarting connection to server...");
  if (client.connect(server, serverPort)) {
    logln("connected to server");
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject& reading = jsonBuffer.createObject();
    reading["t"] = thermocoupleTemp;
    reading["c"] = coldJunctionTemp;

    String readingString;
    reading.printTo(readingString);
    logln("Sending json: " + readingString + " to " + sendReadingRequest);
    
    client.println(sendReadingRequest);
    client.println(hostString);
    client.println(connectionClose);
    client.println("Content-Type: application/json");
    client.print("Content-Length: "); client.println(readingString.length());
    client.println();
    client.println(readingString);
  }else{
    logln("\nUnable to connect to server.  Skipping send.");
  }
}

void readClientData() {
    // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
}

void sendReset() {
  client.stop();
  logln("Starting new readings session");
  if(client.connect(server, serverPort)) {
    logln("connected to server");
    
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    String payload;
    json.printTo(payload);
    logln("Sending json: " + payload);
    
    client.println(resetRequest);
    client.println(hostString);
    client.println(connectionClose);
    client.println("Content-Type: application/json");
    client.print("Content-Length: "); client.println(payload.length());
    client.println();
    client.println(payload);
  }else{
    logln("\nUnable to connect to server. Session not reset.");
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  log("SSID: ");
  logln(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  log("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  log("signal strength (RSSI):");
  log(rssi);
  logln(" dBm");
}

void log(String message) {
  if(enableLogging) {
    Serial.print(message);
  }
}

void logln(String message) {
  if(enableLogging) {
    Serial.println(message);
  }
}

void logln(float message) {
  if(enableLogging) {
    Serial.println(message);
  }
}

void logln() {
  if(enableLogging) {
    Serial.println();
  }
}

String twoDigits(int num) {
  String two = "";
  if (num < 10) {
    two += "0";
  }
  return two + num;
}
