/*
 Thermocouple Client

 This sketch connects takes readings from a thermocouple amplifier,
 batches them and periodically POSTS them to a waiting service using
 a WiFiClient
 */

#include <SPI.h>
#include <WiFi101.h>
#include <Adafruit_MAX31856.h>

char ssid[] = "Baer";
char pass[] = "antelope";
int readings[5];
int nextIndex = 0;
int readingInterval = 1000;
bool putPending = false;

int status = WL_IDLE_STATUS;
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
IPAddress server(192,168,0,20);  // numeric IP for Google (no DNS)
//char server[] = "www.google.com";    // name address for Google (using DNS)

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;

// Initialize the thermocouple amplifier
Adafruit_MAX31856 max = Adafruit_MAX31856(10, 11, 12, 13);

void setup() {
  establishSerialConnection();
  connectToWifi();
  startThermocouple();
}

void loop() {
  readThermoCouple();
  publishReadingsIfReady();
  readClientData();
  delay(readingInterval);
}

void establishSerialConnection() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }  
}

void connectToWifi() {
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    while (true); // don't continue:
  }
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();
}

void startThermocouple() {
  max.begin();
  max.setThermocoupleType(MAX31856_TCTYPE_K);

  Serial.print("Thermocouple type: ");
  switch ( max.getThermocoupleType() ) {
    case MAX31856_TCTYPE_B: Serial.println("B Type"); break;
    case MAX31856_TCTYPE_E: Serial.println("E Type"); break;
    case MAX31856_TCTYPE_J: Serial.println("J Type"); break;
    case MAX31856_TCTYPE_K: Serial.println("K Type"); break;
    case MAX31856_TCTYPE_N: Serial.println("N Type"); break;
    case MAX31856_TCTYPE_R: Serial.println("R Type"); break;
    case MAX31856_TCTYPE_S: Serial.println("S Type"); break;
    case MAX31856_TCTYPE_T: Serial.println("T Type"); break;
    case MAX31856_VMODE_G8: Serial.println("Voltage x8 Gain mode"); break;
    case MAX31856_VMODE_G32: Serial.println("Voltage x8 Gain mode"); break;
    default: Serial.println("Unknown"); break;
  }
}

void readThermoCouple() {
  Serial.print("Taking Reading Number ");
  Serial.println(nextIndex + 1);
  Serial.print("Cold Junction Temp: "); Serial.println(max.readCJTemperature());
  Serial.print("Thermocouple Temp: "); Serial.println(max.readThermocoupleTemperature());
  // Check and print any faults
  uint8_t fault = max.readFault();
  if (fault) {
    if (fault & MAX31856_FAULT_CJRANGE) Serial.println("Cold Junction Range Fault");
    if (fault & MAX31856_FAULT_TCRANGE) Serial.println("Thermocouple Range Fault");
    if (fault & MAX31856_FAULT_CJHIGH)  Serial.println("Cold Junction High Fault");
    if (fault & MAX31856_FAULT_CJLOW)   Serial.println("Cold Junction Low Fault");
    if (fault & MAX31856_FAULT_TCHIGH)  Serial.println("Thermocouple High Fault");
    if (fault & MAX31856_FAULT_TCLOW)   Serial.println("Thermocouple Low Fault");
    if (fault & MAX31856_FAULT_OVUV)    Serial.println("Over/Under Voltage Fault");
    if (fault & MAX31856_FAULT_OPEN)    Serial.println("Thermocouple Open Fault");
  }
  readings[nextIndex] = max.readThermocoupleTemperature();
  nextIndex++;
}

void publishReadingsIfReady() {
  int count = sizeof(readings)/sizeof(int);
  if(nextIndex == count) {
    Serial.print("\nPublishing ");
    Serial.print(count);
    Serial.print(" readings.");
    nextIndex = 0;
    sendReadings();
  }
}

void readClientData() {
    // if there are incoming bytes available
  // from the server, read them and print them:
  bool responseProcessed = client.available();
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }

  // if the server's disconnected, stop the client:
  if (responseProcessed && !client.connected()) {
    Serial.println();
    Serial.println("disconnecting from server.");
    putPending = false;
    client.stop();
  }
}

void sendReadings() {
  if(!putPending) {
    Serial.println("\nStarting connection to server...");
    // if you get a connection, report back via serial:
    if (client.connect(server, 9000)) {
      Serial.println("connected to server");
      //-----
      client.println("POST /hob/bubba/put?reading=999 HTTP/1.1");
      client.println("Host: 192.168.0.20");
      client.println("User-Agent: ArduinoWiFi/1.1");
      client.println("Connection: close");
      client.println();
      //-----
      putPending = true;
    }
  }else{
    Serial.println("\nSkipping send.  Put request already pending.");
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
