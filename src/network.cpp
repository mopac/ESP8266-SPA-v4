#include <Arduino.h>
#include "esp8266_spa.h"
#include <SoftwareSerial.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include "file_system.h"

extern bool loopbackEnable;
extern struct ConnectType Connect;
extern String AP_NamePrefix;
extern uint8_t last_state_crc;
extern uint8_t send;
extern uint8_t testSend;
extern uint8_t settemp;
extern uint8_t CurrentMins;
extern uint8_t CurrentHours;
extern char have_config;
extern char have_faultlog;
extern struct SpaStateType SpaState;
extern struct SpaConfigType SpaConfig;


extern ESP8266WebServer httpServer;
extern PubSubClient mqtt;
extern SoftwareSerial swSer1;
extern void _yield();
extern void hardreset();
// extern void setGlobals(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4 );
// extern void callback(char* p_topic, byte * p_payload, unsigned int p_length);



static const char rootMessage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body>
<h2>%s WIFI Setup</h2>
<form action="/setup" method="POST">
  <label for="ssid">WIFI SSID:</label><br>
  <input type="text" id="ssid" name="ssid" value="%s" size="20"><br>
  <label for="password">Password:</label><br>
  <input type="text" id="password" name="password" value="********" size="20"><br><br>
  <label for="broker">MQTT Broker Address:</label><br>
  <input type="text" id="broker" name="broker" value="%s" size="20"><br>
  <br><p>Leave the next two blank if your MQTT brokers doesn't expect a userid and password</p>
  <label for="broker">MQTT Broker UserID:</label><br>
  <input type="text" id="userid" name="userid" value="%s" size="20"><br>
  <label for="broker">MQTT Broker Password:</label><br>
  <input type="text" id="brokerPassword" name="brokerPassword" value="%s" size="20"><br><br>
  <input type="submit" value="Submit">
</form> 
<p>Click the "Submit" button to update the WIFI setup. The module will reset, this might take 30 seconds</p><br><br>
<p>%s</p><br><br>
<a href="/update">Click here for firmware update</a>
<br><br>
<a href="/loopback">Click here to enable loopback test mode</a>
</body>
</html>
)=====";


// function to show which device it is on web server
void handleRoot() {
char rootMessBuf[sizeof(rootMessage)+200];
char brokerMess[80];
  loopbackEnable = false; // turn off loopback mode
  if(mqtt.connected()) {
    strcpy(brokerMess, "<p style=\"color:Green;\">MQTT Client is connected to Broker</p>");
  } else {
    strcpy(brokerMess, "<p style=\"color:Red;\">[ERROR] MQTT Client NOT connected to Broker</p>");
  }
  sprintf(rootMessBuf, rootMessage, MQTT_TOPIC ,Connect.ssid, Connect.brokerAddress, Connect.brokerUserid, Connect.brokerPassword, brokerMess);
  httpServer.send(200, "text/html", rootMessBuf);
}

// function to enable loopback
void handleLoopback() {
  loopbackEnable = true; // turn on loopback mode
  httpServer.send(200, "text/html", "<h1>Loopback Mode Enabled</h1><br><br><br>Click here to turn off loopback <a href=\"/\">Home - Setup</a>");
}


void handleSetup() {                         // If a POST request is made to URI /setup
  if(      ! httpServer.hasArg("ssid") 
        || ! httpServer.hasArg("password") 
        || ! httpServer.hasArg("broker") 
        || ! httpServer.hasArg("userid") 
        || ! httpServer.hasArg("brokerPassword") 
        || httpServer.arg("ssid") == NULL 
        || httpServer.arg("password") == NULL 
        || httpServer.arg("broker") == NULL
    ) { // If the POST request doesn't the correct data
    httpServer.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    return;
  } 
  strcpy(Connect.ssid, httpServer.arg("ssid").c_str());
  writeFile("/ssid.txt", Connect.ssid);
  if (httpServer.arg("password") != "********") {
    strcpy(Connect.password, httpServer.arg("password").c_str());
    writeFile("/password.txt", Connect.password);
  }  
  //SERUSB.printf("\nRaw Broker: %s",httpServer.arg("broker"));
  //{
  //  char temp[20];
  //  strcpy(temp,httpServer.arg("broker").c_str());
  //  for(int i = 0 ; i < strlen(temp);i++) SERUSB.printf("\nIndex:%d Hex:%x Dec:%d",i,temp[i],temp[i]);
  //  SERUSB.printf("\nTemp= %s",temp);
  //  strcpy(Connect.brokerAddress,temp);
  //}
  strcpy(Connect.brokerAddress,httpServer.arg("broker").c_str());
  writeFile("/broker.txt", Connect.brokerAddress);

  strcpy(Connect.brokerUserid,httpServer.arg("userid").c_str());
  writeFile("/userid.txt", Connect.brokerUserid);

  strcpy(Connect.brokerPassword,httpServer.arg("brokerPassword").c_str());
  writeFile("/brokpass.txt", Connect.brokerPassword);

  SERUSB.printf("\nSetup Data Received: SSID:%s Password:%s Broker:%s Broker UserID:%s Broker Password:%s\n", 
                    Connect.ssid, Connect.password, Connect.brokerAddress, Connect.brokerUserid, Connect.brokerPassword);
  httpServer.send(200, "text/html", "<h1>Update successful</h1><br>Restarting<br><br><a href=\"/\">Home - Setup</a>");
  delay(1000);
  ESP.restart();
}

void handleNotFound(){
  httpServer.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

String getAPName() {
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  return AP_NamePrefix + macID;
}

void mqttpubsub() {
  // ONLY DO THE FOLLOWING IF have_config == true otherwise it will not work
  String Payload;

  // ... Hassio autodiscover
  #ifdef HASSIO 

      //clear topics:
      mqtt.publish("homeassistant/binary_sensor/"MQTT_TOPIC, "");
      mqtt.publish("homeassistant/sensor/"MQTT_TOPIC, "");
      mqtt.publish("homeassistant/switch/"MQTT_TOPIC, "");
      mqtt.publish("/"MQTT_TOPIC, "");

      //temperature -> can we try and remove the Payload below, it's messy
      Payload = "{\"name\":\"Hot tub status\",\"uniq_id\":\"ESP82Spa_1\",\"stat_t\":\MQTT_TOPIC"/node/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"],\"name\":\"Esp Spa\",\"sw\":\""+String(VERSION)+"\"}}";
      mqtt.publish("homeassistant/binary_sensor/"MQTT_TOPIC"/state/config", Payload.c_str());
      //temperature
      //mqtt.publish("homeassistant/sensor/"MQTT_TOPIC"/temperature/config", "{\"name\":\"Hot tub temperature\",\"uniq_id\":\"ESP82Spa_2\",\"stat_t\":\MQTT_TOPIC"/temperature/state\",\"unit_of_meas\":\"°C\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      //target_temperature
      //mqtt.publish("homeassistant/switch/"MQTT_TOPIC"/target_temp/config", "{\"name\":\"Hot tub target temperature\",\"cmd_t\":\MQTT_TOPIC"/target_temp/set\",\"stat_t\":\MQTT_TOPIC"/target_temp/state\",\"unit_of_measurement\":\"°C\"}");
      //climate temperature
      mqtt.publish("homeassistant/climate/"MQTT_TOPIC"/temperature/config", "{\"name\":\"Hot tub thermostat\",\"uniq_id\":\"ESP82Spa_0\",\"temp_cmd_t\":\MQTT_TOPIC"/target_temp/set\",\"mode_cmd_t\":\MQTT_TOPIC"/heat_mode/set\",\"mode_stat_t\":\MQTT_TOPIC"/heat_mode/state\",\"curr_temp_t\":\MQTT_TOPIC"/temperature/state\",\"temp_stat_t\":\MQTT_TOPIC"/target_temp/state\",\"min_temp\":\"27\",\"max_temp\":\"40\",\"modes\":[\"off\", \"heat\"], \"temp_step\":\"0.5\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      //heat mode
      mqtt.publish("homeassistant/switch/"MQTT_TOPIC"/heatingmode/config", "{\"name\":\"Hot tub heating mode\",\"uniq_id\":\"ESP82Spa_3\",\"cmd_t\":\MQTT_TOPIC"/heatingmode/set\",\"stat_t\":\MQTT_TOPIC"/heatingmode/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      //heating state
      mqtt.publish("homeassistant/binary_sensor/"MQTT_TOPIC"/heatstate/config", "{\"name\":\"Hot tub heating state\",\"uniq_id\":\"ESP82Spa_6\",\"stat_t\":\MQTT_TOPIC"/heatstate/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      //high range
      mqtt.publish("homeassistant/switch/"MQTT_TOPIC"/highrange/config", "{\"name\":\"Hot tub high range\",\"uniq_id\":\"ESP82Spa_4\",\"cmd_t\":\MQTT_TOPIC"/highrange/set\",\"stat_t\":\MQTT_TOPIC"/highrange/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");

      //OPTIONAL ELEMENTS
      if (SpaConfig.circ){
        //circulation pump
        mqtt.publish("homeassistant/binary_sensor/"MQTT_TOPIC"/circ/config", "{\"name\":\"Hot tub circulation pump\",\"uniq_id\":\"ESP82Spa_5\",\"device_class\":\"power\",\"stat_t\":\MQTT_TOPIC"/circ/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      }
      if (SpaConfig.light1) {
        //light 1
        mqtt.publish("homeassistant/switch/"MQTT_TOPIC"/light/config", "{\"name\":\"Hot tub light\",\"uniq_id\":\"ESP82Spa_7\",\"cmd_t\":\MQTT_TOPIC"/light/set\",\"stat_t\":\MQTT_TOPIC"/light/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      }
      if (SpaConfig.pump1 != 0) {
        //jets 1
        mqtt.publish("homeassistant/switch/"MQTT_TOPIC"/jet_1/config", "{\"name\":\"Hot tub jet1\",\"uniq_id\":\"ESP82Spa_8\",\"cmd_t\":\MQTT_TOPIC"/jet_1/set\",\"stat_t\":\MQTT_TOPIC"/jet_1/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      }
      if (SpaConfig.pump2 != 0) {
        //jets 2
        mqtt.publish("homeassistant/switch/"MQTT_TOPIC"/jet_2/config", "{\"name\":\"Hot tub jet2\",\"uniq_id\":\"ESP82Spa_9\",\"cmd_t\":\MQTT_TOPIC"/jet_2/set\",\"stat_t\":\MQTT_TOPIC"/jet_2/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      }
      if (SpaConfig.blower)
      {
        //blower
        mqtt.publish("homeassistant/switch/"MQTT_TOPIC"/blower/config", "{\"name\":\"Hot tub blower\",\"uniq_id\":\"ESP82Spa_10\",\"cmd_t\":\MQTT_TOPIC"/blower/set\",\"stat_t\":\MQTT_TOPIC"/blower/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}");
      }

  #endif

  mqtt.publish(MQTT_TOPIC"/node/state", "ON");
  mqtt.publish(MQTT_TOPIC"/node/debug", "RECONNECT");
  //mqtt.publish(MQTT_TOPIC"/node/debug", String(millis()).c_str());
  //mqtt.publish(MQTT_TOPIC"/node/debug", String(oldstate).c_str());
  mqtt.publish(MQTT_TOPIC"/node/version", VERSION);
  mqtt.publish(MQTT_TOPIC"/node/flashsize", String(ESP.getFlashChipRealSize()).c_str());
  mqtt.publish(MQTT_TOPIC"/node/chipid", String(ESP.getChipId()).c_str());
  mqtt.publish(MQTT_TOPIC"/node/speed", String(ESP.getCpuFreqMHz()).c_str());

  // ... and resubscribe
  mqtt.subscribe(MQTT_TOPIC"/command");
  mqtt.subscribe(MQTT_TOPIC"/target_temp/set");
  mqtt.subscribe(MQTT_TOPIC"/heatingmode/set");
  mqtt.subscribe(MQTT_TOPIC"/heat_mode/set");
  mqtt.subscribe(MQTT_TOPIC"/highrange/set");
  mqtt.subscribe(MQTT_TOPIC"/time/set");
  mqtt.subscribe(MQTT_TOPIC"/opmode/set");

  //OPTIONAL ELEMENTS
  if (SpaConfig.pump1 != 0) {
    mqtt.subscribe(MQTT_TOPIC"/jet_1/set");
  }
  if (SpaConfig.pump2 != 0) {
    mqtt.subscribe(MQTT_TOPIC"/jet_2/set");
  }
  if (SpaConfig.blower) {
    mqtt.subscribe(MQTT_TOPIC"/blower/set");
  }
  if (SpaConfig.light1) {
    mqtt.subscribe(MQTT_TOPIC"/light/set");
  }

  mqtt.subscribe(MQTT_TOPIC"/aux1/set");
  mqtt.subscribe(MQTT_TOPIC"/aux2/set");

  mqtt.subscribe(MQTT_TOPIC"/relay_1/set");
  mqtt.subscribe(MQTT_TOPIC"/relay_2/set");

  //not sure what this is
  last_state_crc = 0x00;

  //done with config
  have_config = 3;
}

void reconnect() {
  //int oldstate = mqtt.state();
  //boolean connection = false;
  // Loop until we're reconnected
  SERUSB.println("Starting Reconnect subroutine");
  if (!mqtt.connected()) {
    // Attempt to connect
    SERUSB.println("Looping till connected");
    if (strlen(Connect.brokerPassword) == 0) { // No password so connect without user name and password
      SERUSB.print("Connecting to the MQTT broker...............");
      if (!mqtt.connect(String(String(MQTT_TOPIC) + String(millis())).c_str())) SERUSB.println("Failed mqtt.connect");
      if (mqtt.connected()) SERUSB.println("Connected successfully");
      else SERUSB.println("FAILED TO CONNECT!");
    }
    else {
      SERUSB.print("Connecting to the MQTT broker with username and password...............");
      if (!mqtt.connect((String(String(MQTT_TOPIC) + String(millis())).c_str()), Connect.brokerUserid, Connect.brokerPassword)) SERUSB.println("Failed mqtt.connect");
//      mqtt.connect("Spa1", Connect.brokerUserid, Connect.brokerPassword);
      if (mqtt.connected()) SERUSB.println("Connected successfully");
      else SERUSB.println("FAILED TO CONNECT!");
    }

    if (have_config == 3) {
      have_config = 2; // we have disconnected, let's republish our configuration
    }

  }
  mqtt.setBufferSize(512); //increase pubsubclient buffer size
  mqtt.publish(MQTT_TOPIC"/node/debug", "Connected to MQTT Broker");
}



