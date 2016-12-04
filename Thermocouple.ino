/*
 Thermocouple Client

 This sketch connects takes readings from a thermocouple amplifier,
 batches them and periodically POSTS them to a waiting service using
 a WiFiClient
 */

#include <SPI.h>
#include <WiFi101.h>
#include <RTCZero.h>
#include <Adafruit_MAX31856.h>
#include <ArduinoJson.h>

const char ssid[] = "Baer";
const char pass[] = "antelope";
const int batchSize = 5;
const int readingInterval = 30000;
const bool enableLogging = false;

RTCZero rtc;
float thermocoupleTemps[batchSize];
float coldJunctionTemps[batchSize];
long timeStamps[batchSize];

int nextIndex = 0;
int status = WL_IDLE_STATUS;
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(192,168,0,20);  // numeric IP for Google (no DNS)
char server[] = "ec2-54-149-249-84.us-west-2.compute.amazonaws.com";

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;

// Initialize the thermocouple amplifier
Adafruit_MAX31856 max = Adafruit_MAX31856(10, 11, 12, 13);

void setup() {
  establishSerialConnection();
  connectToWifi();
  establishTime();
  startThermocouple();
}

void loop() {
  readThermoCouple();
  publishReadingsIfReady();
  readClientData();
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

void establishTime() {
  rtc.begin();
  unsigned long epoch;
  int numberOfTries = 0, maxTries = 6;
  do {
    epoch = WiFi.getTime();
    numberOfTries++;
  }
  while ((epoch == 0) || (numberOfTries > maxTries));

  if (numberOfTries > maxTries) {
    log("NTP unreachable!!");
    while (1);
  }
  else {
    log("Epoch received: ");
    logln(epoch);
    rtc.setEpoch(epoch);

    logln();
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

  log("Thermocouple type: ");
  switch ( max.getThermocoupleType() ) {
    case MAX31856_TCTYPE_B: logln("B Type"); break;
    case MAX31856_TCTYPE_E: logln("E Type"); break;
    case MAX31856_TCTYPE_J: logln("J Type"); break;
    case MAX31856_TCTYPE_K: logln("K Type"); break;
    case MAX31856_TCTYPE_N: logln("N Type"); break;
    case MAX31856_TCTYPE_R: logln("R Type"); break;
    case MAX31856_TCTYPE_S: logln("S Type"); break;
    case MAX31856_TCTYPE_T: logln("T Type"); break;
    case MAX31856_VMODE_G8: logln("Voltage x8 Gain mode"); break;
    case MAX31856_VMODE_G32: logln("Voltage x8 Gain mode"); break;
    default: logln("Unknown"); break;
  }
}

void readThermoCouple() {
  log("Taking Reading Number ");
  logln(nextIndex + 1);
  log("Cold Junction Temp: "); logln(max.readCJTemperature());
  log("Thermocouple Temp: "); logln(max.readThermocoupleTemperature());
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

  thermocoupleTemps[nextIndex] = max.readThermocoupleTemperature();
  coldJunctionTemps[nextIndex] = max.readCJTemperature();
  timeStamps[nextIndex] = rtc.getEpoch();
  nextIndex++;
}

void publishReadingsIfReady() {
  if(nextIndex == batchSize) {
    log("\nPublishing ");
    log(batchSize);
    log(" readings.");
    sendReadings();
    nextIndex = 0;
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

void sendReadings() {
  client.stop();
  logln("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    logln("connected to server");

    StaticJsonBuffer<500> jsonBuffer;
    JsonArray& jsonReadings = jsonBuffer.createArray();
    for(int i=0; i<batchSize; i++) {
      JsonObject& reading = jsonBuffer.createObject();
      reading["t"] = thermocoupleTemps[i];
      reading["c"] = coldJunctionTemps[i];
      reading["d"] = timeStamps[i];
      jsonReadings.add(reading);
    }

    String readingsString;
    jsonReadings.printTo(readingsString);
    logln("Sending json: " + readingsString);

    client.println("POST /hob/bubba/put HTTP/1.1");
    client.println("Host: ec2-54-149-249-84.us-west-2.compute.amazonaws.com");
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: "); client.println(readingsString.length());
    client.println();
    client.println(readingsString);
  }else{
    logln("\nSkipping send.  Put request already pending.");
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  log("SSID: ");
  logln(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  log("IP Address: ");
  logln(ip);

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

void logln(int message) {
  if(enableLogging) {
    Serial.println(message);
  }
}

void logln() {
  if(enableLogging) {
    Serial.println();
  }
}

