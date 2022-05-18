
// https://github.com/ccutrer/balboa_worldwide_app/blob/master/doc/protocol.md
// Reference:https://github.com/ccutrer/balboa_worldwide_app/wiki

// Please install the needed dependencies:
// CircularBuffer
// PubSubClient
// EspSoftwareSerial


// +12V RED
// GND  BLACK
// A    YELLOW
// B    WHITE

#include "SoftwareSerial.h"
#include "bilboa_helper.h"
#include "esp8266_spa.h"
#include "protocol.h"
#include "file_system.h"
#include "network.h"


struct ConnectType Connect;

String AP_NamePrefix = MQTT_TOPIC" ";
const char* DomainName = MQTT_TOPIC;  // set domain name domain.local
bool stationMode = true;

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

SoftwareSerial swSer1;

CircularBuffer<uint8_t, 35> Q_in;
CircularBuffer<uint8_t, 35> Q_out;

uint8_t last_state_crc = 0x00;
uint8_t send = 0x00;
uint8_t settemp = 0x00;
uint8_t CurrentMins = 0;
uint8_t CurrentHours = 0;
uint8_t counter = 0;

unsigned long lastrx = 0;
unsigned long lastmdns = 0;
unsigned long loopbackTime = 0;
bool loopbackEnable = false;

char have_config = 0; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
char have_faultlog = 0; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
char faultlog_minutes = 0; //temp logic so we only get the fault log once per 5 minutes
char ip_settings = 0; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it

float loopbackValue = 0;

struct SpaStateType SpaState;
struct SpaConfigType SpaConfig;

void _yield() {
  yield();
  mqtt.loop();
  httpServer.handleClient();
  MDNS.update();
}

void print_msg(CircularBuffer<uint8_t, 35> &data) {
  String s;
  uint8_t x;
  //for (i = 0; i < (Q_in[1] + 2); i++) {
  for (uint8_t i = 0; i < data.size(); i++) {
    x = Q_in[i];
    if (x < 0x0A) s += "0";
    s += String(x, HEX);
    s += " ";
  }
  mqtt.publish(MQTT_TOPIC"/node/msg", s.c_str());
  _yield();
}


///////////////////////////////////////////////////////////////////////////////

void hardreset() {
  ESP.wdtDisable();
  while (1) {};
}



///////////////////////////////////////////////////////////////////////////////

void setup() {

#ifdef SWAP
  // Using hardware Serial for RS485 and software Serial for USB
  Serial.begin(115200);
  Serial.swap(); // Switch Hardware Serial to GPIO13 & GPIO15

  swSer1.begin(57600,SWSERIAL_8N1, 3, 1, false); // Start software serial
  swSer1.enableIntTx(false); // This is needed with high baudrates

  if (!swSer1) { // If the object did not initialize, then its configuration is invalid
  Serial.swap(); //Switch the Serial back to 1 & 3
  Serial.println("");
  Serial.println("Invalid SoftwareSerial pin configuration, check config"); 
  }
  else {
  swSer1.println("");
  swSer1.println("Correct SoftwareSerial pin configuration. Using Software Serial for USB");   
  } 
#else
  // Using Software Serial for RS485 and Hardware Serial for USB
  // Spa communication, 115.200 baud 8N1
  Serial.begin(57600);
  
  swSer1.begin(115200,SWSERIAL_8N1, 13, 15, true);
  swSer1.enableIntTx(false);

  if (!swSer1) { // If the object did not initialize, then its configuration is invalid
  Serial.println("");
  Serial.println("Invalid SoftwareSerial pin configuration, check config"); 
  }
  else {
  Serial.println("");
  Serial.println("Correct SoftwareSerial pin configuration. Using Software Serial for RS485");   
  } 
#endif

  SERUSB.println("");
  SERUSB.println("ESP8266 SPA........Starting");

//  fsSetup(Connect.ssid, Connect.password, Connect.brokerAddress, Connect.brokerUserid, Connect.brokerPassword); //Read the wifi details from File System
  fsSetup(&Connect); //Read the wifi details from File System

  RS485setup(); // Setup the RS485 interface

  // give Spa time to wake up after POST
  for (uint8_t i = 0; i < 5; i++) {
    delay(1000);
    yield();
  }
  SERUSB.println("5 second delay elapsed");
  Q_in.clear();
  Q_out.clear();

  // WiFi.setOutputPower(20.5); // this sets wifi to highest power
  WiFi.begin(Connect.ssid, Connect.password);
  SERUSB.println("Starting wifi");
  SERUSB.print("SSID = ");
  SERUSB.print(Connect.ssid);
  SERUSB.print("    Password = ");
  SERUSB.println(Connect.password);

  int i = 0;
  while ((WiFi.status() != WL_CONNECTED) && (i<15)) { // Wait for the Wi-Fi to connect
    delay(1000);
    SERUSB.print(++i); SERUSB.print(' ');
    yield();
  }

  
  if (i>=15) { // Connection timed out
    stationMode = false;
    SERUSB.println("wifi timeout starting SoftAP");
  //  hardreset();
    WiFi.disconnect();  // Turn off station mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(getAPName().c_str());
    if (!MDNS.begin(DomainName, WiFi.softAPIP())) {
    SERUSB.println("[ERROR] MDNS responder did not setup");
    } else {
      SERUSB.printf("[INFO] MDNS setup is successful! Domain Name: %s.local\n", DomainName);
    }
  }
  else { // Connected successfully
    SERUSB.println('\n');
    SERUSB.println("Connection established!");  
    SERUSB.print("IP address:\t");
    SERUSB.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer  
    SERUSB.print("Starting MDNS..........");  
    if(!MDNS.begin(DomainName, WiFi.localIP())) SERUSB.printf("[ERROR] mDNS failed to start\n");
    else SERUSB.printf("successful. Domain Name: %s.local\n", DomainName);
  }

  SERUSB.println("Starting updater");  
  httpServer.on("/", handleRoot);
  httpServer.on("/setup", handleSetup);
  httpServer.on("/loopback", handleLoopback);
  httpServer.onNotFound(handleNotFound);

  httpUpdater.setup(&httpServer, "admin", "");
  
  const char * headerkeys[] = {"User-Agent", "Cookie"};
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);

  httpServer.collectHeaders(headerkeys, headerkeyssize);

  SERUSB.println("Starting web server");  
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);

  if(i<15) { // Only start mqtt if wifi connected

  IPAddress resolvedIP;
    if (!WiFi.hostByName(Connect.brokerAddress, resolvedIP)) {
      SERUSB.printf("DNS lookup failed for:%s\n", Connect.brokerAddress);
    }
    SERUSB.print(Connect.brokerAddress);
    SERUSB.print(" IP: ");
    SERUSB.println(resolvedIP);

    SERUSB.println("Setting Broker name"); 
    mqtt.setServer(resolvedIP, 1883);
    SERUSB.println("Set Callback");  
    mqtt.setCallback(callback);
    SERUSB.println("Set Keep alive");  
    mqtt.setKeepAlive(10);
    SERUSB.println("Set socket timeout");  
    mqtt.setSocketTimeout(20);
  }

  loopbackTime = millis(); // start loopback timer

}


void loop() {
  if(!stationMode) { //Must be in AP mode
    _yield(); 
  }
  else {
    if (WiFi.status() != WL_CONNECTED) {
      SERUSB.println("Not connected to wifi - reset");  
      delay(1000);
      ESP.restart();
    } 
    if (!mqtt.connected()) reconnect();
    if (have_config == 2) mqttpubsub(); //do mqtt stuff after we're connected and if we have got the config elements
    //httpServer.handleClient(); needed?
    _yield();


    //Every x minutes, read the fault log using SpaState,minutes
    if (SpaState.minutes % 5 == 0)
    {
      
      //logic to only get the error message once -> this is dirty
      //have_faultlog = 0;
      if (have_faultlog == 2) { // we got the fault log before and treated it
        if (faultlog_minutes == SpaState.minutes) { // we got the fault log this interval so do nothing
        }
        else {
          faultlog_minutes = SpaState.minutes;
          have_faultlog = 0;
          mqtt.publish(MQTT_TOPIC"/node/msg", "Schedule Fault Log request");
        }
      }
    }
    
    if (spaReceive()) { // Check if complete serial messages have arrived
      protocolParser();
    } 

    _yield();

    // Long time no receive
    if (millis() - lastrx > 5000) {
      lastrx = millis();
      mqtt.publish(MQTT_TOPIC"/node/msg", "No message for 5 secs");  
      // hardreset();
    }

    if ((millis() - loopbackTime > 3000) && (loopbackEnable == true)) { // loopback mode and 3 seconds has elasped

      loopbackTime = millis();
      mqtt.publish(MQTT_TOPIC"/node/msg", "Sending ID Loopback"); 
      SERUSB.printf("\nSending ID Loopback: Value:%2.1f\n",loopbackValue); 
      spa_emulate();
      mqtt.publish(MQTT_TOPIC"/time/state", String(loopbackValue, 2).c_str());
      mqtt.publish(MQTT_TOPIC"/temperature/state", String(loopbackValue, 2).c_str());
      mqtt.publish(MQTT_TOPIC"/target_temp/state", String(loopbackValue, 2).c_str());
      loopbackValue++;
      if (loopbackValue > 99) loopbackValue = 0;
    }

  }  
}
