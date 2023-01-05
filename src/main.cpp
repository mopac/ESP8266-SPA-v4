
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

#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <WebServer.h>   // Local WebServer used to serve the configuration portal
  #include <ESPmDNS.h>
  #include <HTTPUpdateServer.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
  #include <ESP8266mDNS.h>
  #include <ESP8266HTTPUpdateServer.h>
#endif

#include <ArduinoOTA.h>

unsigned long lastrx = 0;
unsigned long lastmdns = 0;
unsigned long loopbackTime = 0;
bool loopbackEnable = false;

char have_config = 0; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
char have_faultlog = 0; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
char faultlog_minutes = 0; //temp logic so we only get the fault log once per 5 minutes
char ip_settings = 0; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it

float loopbackValue = 0;

uint8_t last_state_crc = 0x00;
uint8_t id = 0x00;
volatile uint8_t send = 0x00;
volatile uint8_t testSend = 0;
volatile uint8_t settemp = 0x00;
volatile uint8_t CurrentMins = 0;
volatile uint8_t CurrentHours = 0;
volatile struct CallDataType callData;
volatile struct CallDataType *callDataPtr = &callData;

struct SpaStateType SpaState;
struct SpaConfigType SpaConfig;

CircularBuffer<uint8_t, 40> Q_in;
CircularBuffer<uint8_t, 40> Q_out;

#if defined(ARDUINO_ARCH_ESP32)
  WebServer httpServer(80);
  HTTPUpdateServer httpUpdater;
#else
  ESP8266WebServer httpServer(80);
  ESP8266HTTPUpdateServer httpUpdater;
#endif

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

SoftwareSerial swSer1;

void setGlobals(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4 )
{
  send = v1;
  CurrentHours = v2;
  CurrentMins = v3;
  settemp = v4;
  callData.send = v1;
  callData.CurrentHours = v2;
  callData.CurrentMins = v3;
  callData.settemp = v4;
      {
        char temp[128];
        sprintf(temp, "main: setGlobals send = 0x%x settemp = %d CurrentHours = %d CurrentMins = %d", send, settemp, CurrentHours, CurrentMins);
        mqtt.publish(MQTT_TOPIC"/node/debug", temp);
      }

}


void _yield() {
  yield();
  mqtt.loop();
  httpServer.handleClient();
  #if defined(ARDUINO_ARCH_ESP32)

  #else  
    MDNS.update();
  #endif
}

void print_msg(CircularBuffer<uint8_t, 40> &data) {
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
  SERUSB.println("Oh Dear! something has gone wrong");
  SERUSB.flush();
  delay(10);
  #if defined(ARDUINO_ARCH_ESP32)
    esp_restart();
  #else
    ESP.wdtDisable();
    while (1) {};
  #endif
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte * p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  String topic = String(p_topic);

  mqtt.publish(MQTT_TOPIC"/node/debug", topic.c_str());
  _yield();

  // handle message topic
  if (topic.startsWith(MQTT_TOPIC"/relay_")) {
    bool newstate = 0;

    if (payload.equals("ON")) newstate = LOW;
    else if (payload.equals("OFF")) newstate = HIGH;

    if (topic.charAt(10) == '1') {
      pinMode(RLY1, INPUT);
      delay(25);
      pinMode(RLY1, OUTPUT);
      digitalWrite(RLY1, newstate);
    }
    else if (topic.charAt(10) == '2') {
      pinMode(RLY2, INPUT);
      delay(25);
      pinMode(RLY2, OUTPUT);
      digitalWrite(RLY2, newstate);
    }
  } else if (topic.equals(MQTT_TOPIC"/command")) {
    if (payload.equals("reset")) hardreset();
  } else if (topic.equals(MQTT_TOPIC"/heatingmode/set")) {
    if (payload.equals("READY") && SpaState.restmode == 1) send = 0x51; // toggle to READY
    else if (payload.equals("REST") && SpaState.restmode == 0) send = 0x51; //toggle to REST
  } else if (topic.equals(MQTT_TOPIC"/heat_mode/set")) {
    if (payload.equals("heat") && SpaState.restmode == 1) send = 0x51; // ON = Ready; OFF = Rest
    else if (payload.equals("off") && SpaState.restmode == 0) send = 0x51;
  } else if (topic.equals(MQTT_TOPIC"/light/set")) {
    mqtt.publish(MQTT_TOPIC"/node/debug", "Set Light Send = 0x11");
    if (payload.equals("ON") && SpaState.light == 0) send = 0x11;  
    else if (payload.equals("OFF") && SpaState.light == 1) send = 0x11;
  } else if (topic.equals(MQTT_TOPIC"/aux1/set")) {
    send = 0x16;
    mqtt.publish(MQTT_TOPIC"/node/debug", "aux1 set 0x16");
  } else if (topic.equals(MQTT_TOPIC"/aux2/set")) {
    send = 0x17;
    mqtt.publish(MQTT_TOPIC"/node/debug", "aux2 set 0x17");
  } else if (topic.equals(MQTT_TOPIC"/jet_1/set")) {
    if (payload.equals("ON") && SpaState.jet1 == 0) send = 0x04;
    else if (payload.equals("OFF") && SpaState.jet1 == 1) send = 0x04;
  } else if (topic.equals(MQTT_TOPIC"/jet_2/set")) {
    if (payload.equals("ON") && SpaState.jet2 == 0) send = 0x05;
    else if (payload.equals("OFF") && SpaState.jet2 == 1) send = 0x05;
  } else if (topic.equals(MQTT_TOPIC"/blower/set")) {
    if (payload.equals("ON") && SpaState.blower == 0) send = 0x0C;
    else if (payload.equals("OFF") && SpaState.blower == 1) send = 0x0C;
  } else if (topic.equals(MQTT_TOPIC"/highrange/set")) {
    if (payload.equals("HIGH") && SpaState.highrange == 0) send = 0x50; // toggle to HIGH
    else if (payload.equals("LOW") && SpaState.highrange == 1) send = 0x50; // toggle to LOW
  } else if (topic.equals(MQTT_TOPIC"/target_temp/set")) {
    // Get new set temperature
    double d = payload.toDouble();
    if (d > 0) d *= 2; // Convert to internal representation
    settemp = (uint8_t)d;
    send = 0xff; // Marker to show 'Set Temp'
  } else if (topic.equals(MQTT_TOPIC"/time/set")) {
    // Get new time
    int16_t CurrentTime = payload.toInt(); // Minutes from midnight
    CurrentHours = (uint8_t)(CurrentTime / 60);
    CurrentMins = (uint8_t)(CurrentTime % 60);
    char msg[128];
    sprintf(msg,"Set Time Call: CurrentTime = %d, CurrentHours = %d, CurrentMins = %d", CurrentTime,CurrentHours,CurrentMins);
    mqtt.publish(MQTT_TOPIC"/node/debug", msg);
    if ((CurrentHours < 24) && (CurrentMins < 60 )) send = 0xFE;  // Marker to show 'Set time'
  }  else if (topic.equals(MQTT_TOPIC"/opmode/set")) {
    if (payload.equals("HOLD") && SpaState.opmode == 0) send = 0x3c; // toggle to HOLD
    else if (payload.equals("RUN") && SpaState.opmode == 2) send = 0x3c; // toggle to RUN    
  }   
  callData.send = send;
  callData.CurrentHours = CurrentHours;
  callData.CurrentMins = CurrentMins;
  callData.settemp = settemp;
    {
      char temp[64];
      sprintf(temp, "Callback: At the end of Callback Send = 0x%x", send);
      mqtt.publish(MQTT_TOPIC"/node/debug", temp);
    }
  // setGlobals(send, CurrentHours, CurrentMins, settemp);
}



///////////////////////////////////////////////////////////////////////////////

void setup() {

//  RS485setup(); // Setup the RS485 interface in the disabled state

#if defined(ARDUINO_ARCH_ESP32)
    // Using Serial1 for RS485 and Serial0 for USB
    // Using Serial2 for Spa communication, 115.200 baud 8N1
    SERUSB.begin(19200);
    SERUSB.printf("\nDid we even get to the first line of code?????\n");
    delay(1000);
    SER485.begin(115200, SERIAL_8N1, TX485_RX, TX485_TX);
   
#else
  #ifdef SWAP
    // Using hardware Serial for RS485 and software Serial for USB
    Serial.begin(115200);
    Serial.swap(); // Switch Hardware Serial to GPIO13 & GPIO15

    swSer1.begin(19200,SWSERIAL_8N1, 3, 1, false); // Start software serial
    // swSer1.enableIntTx(false); // This is needed with high baudrates

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
    Serial.begin(19200);
    
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
#endif
  SERUSB.println("");
  SERUSB.println("ESP8266 SPA........Starting");

//  fsSetup(Connect.ssid, Connect.password, Connect.brokerAddress, Connect.brokerUserid, Connect.brokerPassword); //Read the wifi details from File System
  fsSetup(&Connect); //Read the wifi details from File System


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
//    if (!MDNS.begin(DomainName, WiFi.softAPIP())) {
    if (!MDNS.begin(DomainName)) {
  
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
//    if(!MDNS.begin(DomainName, WiFi.localIP())) SERUSB.printf("[ERROR] mDNS failed to start\n");
    if(!MDNS.begin(DomainName)) SERUSB.printf("[ERROR] mDNS failed to start\n");
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

  // give Spa time to wake up after POST
  for (uint8_t i = 0; i < 10; i++) {
    delay(1000);
    yield();
  }
  SERUSB.println("10 second delay elapsed");
  Q_in.clear();
  Q_out.clear();
  while(SER485.available()) SER485.read(); // flush the receive buffer
  delay(10);
  RS485enableRx();

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
//    if (send > 0) {
//      char temp[128];
//      sprintf(temp, "Main: Before Protocolparser is called send = 0x%x settemp = %d CurrentHours = %d CurrentMins = %d", send, settemp, CurrentHours, CurrentMins);
//      mqtt.publish(MQTT_TOPIC"/node/debug", temp);
//    }

//    if (callData.send > 1) {
//      char temp[128];
//      sprintf(temp, "Main: Before Protocolparser Structure send = 0x%x settemp = %d CurrentHours = %d CurrentMins = %d", callData.send, callData.settemp, callData.CurrentHours, callData.CurrentMins);
//      mqtt.publish(MQTT_TOPIC"/node/debug", temp);
//    }
    
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
      mqtt.publish(MQTT_TOPIC"/node/debug", "Sending ID Loopback"); 
      SERUSB.printf("\nSending ID Loopback: Value:%2.1f\n",loopbackValue); 
      if (id == 0x00) spa_emulate_id();
      else spa_emulate_request();
      mqtt.publish(MQTT_TOPIC"/time/state", String(loopbackValue, 2).c_str());
      mqtt.publish(MQTT_TOPIC"/temperature/state", String(loopbackValue, 2).c_str());
      mqtt.publish(MQTT_TOPIC"/target_temp/state", String(loopbackValue, 2).c_str());
      loopbackValue++;
      if (loopbackValue > 99) loopbackValue = 0;
    }

  }  
}
