#include <Arduino.h>
#include "esp8266_spa.h"
#include "bilboa_helper.h"
#include <SoftwareSerial.h>

extern CircularBuffer<uint8_t, 40> Q_in;
extern CircularBuffer<uint8_t, 40> Q_out;
extern PubSubClient mqtt;
extern void _yield();
extern void print_msg(CircularBuffer<uint8_t, 40> &data);
extern uint8_t last_state_crc;
extern uint8_t send;
extern uint8_t settemp;
extern uint8_t CurrentMins;
extern uint8_t CurrentHours;
extern char have_config;
extern char have_faultlog;
extern struct SpaStateType SpaState;
extern struct SpaConfigType SpaConfig;
struct SpaFaultLogType SpaFaultLog;
extern SoftwareSerial swSer1;

extern uint8_t id;


void decodeFault() {
  SpaFaultLog.totEntry = Q_in[5];
  SpaFaultLog.currEntry = Q_in[6];
  SpaFaultLog.faultCode = Q_in[7];
  switch (SpaFaultLog.faultCode) { // this is a inelegant way to do it, a lookup table would be better
    case 15:
      SpaFaultLog.faultMessage = "Sensors are out of sync";
      break;
    case 16:
      SpaFaultLog.faultMessage = "The water flow is low";
      break;
    case 17:
      SpaFaultLog.faultMessage = "The water flow has failed";
      break;
    case 18:
      SpaFaultLog.faultMessage = "The settings have been reset";
      break;
    case 19:
      SpaFaultLog.faultMessage = "Priming Mode";
      break;
    case 20:
      SpaFaultLog.faultMessage = "The clock has failed";
      break;
    case 21:
      SpaFaultLog.faultMessage = "The settings have been reset";
      break;
    case 22:
      SpaFaultLog.faultMessage = "Program memory failure";
      break;
    case 26:
      SpaFaultLog.faultMessage = "Sensors are out of sync -- Call for service";
      break;
    case 27:
      SpaFaultLog.faultMessage = "The heater is dry";
      break;
    case 28:
      SpaFaultLog.faultMessage = "The heater may be dry";
      break;
    case 29:
      SpaFaultLog.faultMessage = "The water is too hot";
      break;
    case 30:
      SpaFaultLog.faultMessage = "The heater is too hot";
      break;
    case 31:
      SpaFaultLog.faultMessage = "Sensor A Fault";
      break;
    case 32:
      SpaFaultLog.faultMessage = "Sensor B Fault";
      break;
    case 34:
      SpaFaultLog.faultMessage = "A pump may be stuck on";
      break;
    case 35:
      SpaFaultLog.faultMessage = "Hot fault";
      break;
    case 36:
      SpaFaultLog.faultMessage = "The GFCI test failed";
      break;
    case 37:
      SpaFaultLog.faultMessage = "Standby Mode (Hold Mode)";
      break;
    default:
      SpaFaultLog.faultMessage = "Unknown error";
      break;
  }
  SpaFaultLog.daysAgo = Q_in[8];
  SpaFaultLog.hour = Q_in[9];
  SpaFaultLog.minutes = Q_in[10];
  mqtt.publish(MQTT_TOPIC"/fault/Entries", String(SpaFaultLog.totEntry).c_str());
  mqtt.publish(MQTT_TOPIC"/fault/Entry", String(SpaFaultLog.currEntry).c_str());
  mqtt.publish(MQTT_TOPIC"/fault/Code", String(SpaFaultLog.faultCode).c_str());
  mqtt.publish(MQTT_TOPIC"/fault/Message", SpaFaultLog.faultMessage.c_str());
  mqtt.publish(MQTT_TOPIC"/fault/DaysAgo", String(SpaFaultLog.daysAgo).c_str());
  mqtt.publish(MQTT_TOPIC"/fault/Hours", String(SpaFaultLog.hour).c_str());
  mqtt.publish(MQTT_TOPIC"/fault/Minutes", String(SpaFaultLog.minutes).c_str());
  have_faultlog = 2;
}

void decodeSettings() {
  mqtt.publish(MQTT_TOPIC"/config/status", "Got config");
  SpaConfig.pump1 = Q_in[5] & 0x03;
  SpaConfig.pump2 = (Q_in[5] & 0x0C) >> 2;
  SpaConfig.pump3 = (Q_in[5] & 0x30) >> 4;
  SpaConfig.pump4 = (Q_in[5] & 0xC0) >> 6;
  SpaConfig.pump5 = (Q_in[6] & 0x03);
  SpaConfig.pump6 = (Q_in[6] & 0xC0) >> 6;
  SpaConfig.light1 = (Q_in[7] & 0x03);
  SpaConfig.light2 = (Q_in[7] >> 2) & 0x03;
  SpaConfig.circ = ((Q_in[8] & 0x80) != 0);
  SpaConfig.blower = ((Q_in[8] & 0x03) != 0);
  SpaConfig.mister = ((Q_in[9] & 0x30) != 0);
  SpaConfig.aux1 = ((Q_in[9] & 0x01) != 0);
  SpaConfig.aux2 = ((Q_in[9] & 0x02) != 0);
  mqtt.publish(MQTT_TOPIC"/config/pumps1", String(SpaConfig.pump1).c_str());
  mqtt.publish(MQTT_TOPIC"/config/pumps2", String(SpaConfig.pump2).c_str());
  mqtt.publish(MQTT_TOPIC"/config/pumps3", String(SpaConfig.pump3).c_str());
  mqtt.publish(MQTT_TOPIC"/config/pumps4", String(SpaConfig.pump4).c_str());
  mqtt.publish(MQTT_TOPIC"/config/pumps5", String(SpaConfig.pump5).c_str());
  mqtt.publish(MQTT_TOPIC"/config/pumps6", String(SpaConfig.pump6).c_str());
  mqtt.publish(MQTT_TOPIC"/config/light1", String(SpaConfig.light1).c_str());
  mqtt.publish(MQTT_TOPIC"/config/light2", String(SpaConfig.light2).c_str());
  mqtt.publish(MQTT_TOPIC"/config/circ", String(SpaConfig.circ).c_str());
  mqtt.publish(MQTT_TOPIC"/config/blower", String(SpaConfig.blower).c_str());
  mqtt.publish(MQTT_TOPIC"/config/mister", String(SpaConfig.mister).c_str());
  mqtt.publish(MQTT_TOPIC"/config/aux1", String(SpaConfig.aux1).c_str());
  mqtt.publish(MQTT_TOPIC"/config/aux2", String(SpaConfig.aux2).c_str());
  have_config = 2;
}

void decodeState() {
  String s;
  double d = 0.0;
  double c = 0.0;

  // DEBUG for finding meaning:
  print_msg(Q_in);

  // 25:Flag Byte 20 - Set Temperature
  d = Q_in[25] / 2;
  if (Q_in[25] % 2 == 1) d += 0.5;
  mqtt.publish(MQTT_TOPIC"/target_temp/state", String(d, 2).c_str());

  // 7:Flag Byte 2 - Actual temperature
  if (Q_in[7] != 0xFF) {
    d = Q_in[7] / 2;
    if (Q_in[7] % 2 == 1) d += 0.5;
    if (c > 0) {
      if ((d > c * 1.2) || (d < c * 0.8)) d = c; //remove spurious readings greater or less than 20% away from previous read
    }

    mqtt.publish(MQTT_TOPIC"/temperature/state", String(d, 2).c_str());
    c = d;
  } else {
    d = 0;
  }
  // REMARK Move upper publish to HERE to get 0 for unknown temperature

  // 8:Flag Byte 3 Hour & 9:Flag Byte 4 Minute => Time
  if (Q_in[8] < 10) s = "0"; else s = "";
  SpaState.hour = Q_in[8];
  s = String(Q_in[8]) + ":";
  if (Q_in[9] < 10) s += "0";
  s += String(Q_in[9]);
  SpaState.minutes = Q_in[9];
  mqtt.publish(MQTT_TOPIC"/time/state", s.c_str());

  // 10:Flag Byte 5 - Heating Mode
  switch (Q_in[10]) {
    case 0:mqtt.publish(MQTT_TOPIC"/heatingmode/state", "READY"); //Ready
//      mqtt.publish(MQTT_TOPIC"/heat_mode/state", "heat"); //Ready
      SpaState.restmode = 0;
      break;
    case 3:mqtt.publish(MQTT_TOPIC"/heatingmode/state", "READY-IN-REST"); // Ready-in-Rest
      SpaState.restmode = 0;
      break;
    case 1:mqtt.publish(MQTT_TOPIC"/heatingmode/state", "REST"); //Rest
//      mqtt.publish(MQTT_TOPIC"/heat_mode/state", "off"); //Rest
      SpaState.restmode = 1;
      break;
  }

  // 15:Flags Byte 10 / Heat status, Temp Range
  // d = bitRead(Q_in[15], 4);
  d = (((Q_in[15]) >> (4)) & 0x03); // bits 4 & 5
  if (d == 0) mqtt.publish(MQTT_TOPIC"/heatstate/state", "OFF");
  else if (d == 1) mqtt.publish(MQTT_TOPIC"/heatstate/state", "HEATING");
  else if (d == 2) mqtt.publish(MQTT_TOPIC"/heatstate/state", "HEAT WAITING");

  d = bitRead(Q_in[15], 2);
  if (d == 0) {
    mqtt.publish(MQTT_TOPIC"/highrange/state", "LOW"); //LOW
    SpaState.highrange = 0;
  } else if (d == 1) {
    mqtt.publish(MQTT_TOPIC"/highrange/state", "HIGH"); //HIGH
    SpaState.highrange = 1;
  }

  // 16:Flags Byte 11
  if (bitRead(Q_in[16], 1) == 1) {
    mqtt.publish(MQTT_TOPIC"/jet_1/state", STRON);
    SpaState.jet1 = 1;
  } else {
    mqtt.publish(MQTT_TOPIC"/jet_1/state", STROFF);
    SpaState.jet1 = 0;
  }

  if (bitRead(Q_in[16], 3) == 1) {
    mqtt.publish(MQTT_TOPIC"/jet_2/state", STRON);
    SpaState.jet2 = 1;
  } else {
    mqtt.publish(MQTT_TOPIC"/jet_2/state", STROFF);
    SpaState.jet2 = 0;
  }

  // 18:Flags Byte 13
  if (bitRead(Q_in[18], 1) == 1)
    mqtt.publish(MQTT_TOPIC"/circ/state", STRON);
  else
    mqtt.publish(MQTT_TOPIC"/circ/state", STROFF);

  if (bitRead(Q_in[18], 2) == 1) {
    mqtt.publish(MQTT_TOPIC"/blower/state", STRON);
    SpaState.blower = 1;
  } else {
    mqtt.publish(MQTT_TOPIC"/blower/state", STROFF);
    SpaState.blower = 0;
  }
  // 19:Flags Byte 14
  if (Q_in[19] == 0x03) {
    mqtt.publish(MQTT_TOPIC"/light/state", STRON);
    SpaState.light = 1;
  } else {
    mqtt.publish(MQTT_TOPIC"/light/state", STROFF);
    SpaState.light = 0;
  }
  // Spa State - Operating Mode
  if (Q_in[5] == 0x00) {
    mqtt.publish(MQTT_TOPIC"/opmode/state", "RUN");
    SpaState.opmode = 0;    
  } else if (Q_in[5] == 0x02) {
    mqtt.publish(MQTT_TOPIC"/opmode/state", "PRIMING");
    SpaState.opmode = 1;    
  } else if (Q_in[5] == 0x05) {
    mqtt.publish(MQTT_TOPIC"/opmode/state", "HOLD");
    SpaState.opmode = 2;    
  }

  last_state_crc = Q_in[Q_in[1]];

  // Publish own relay states
  s = "OFF";
  if (digitalRead(RLY1) == LOW) s = "ON";
  mqtt.publish(MQTT_TOPIC"/relay_1/state", s.c_str());

  s = "OFF";
  if (digitalRead(RLY2) == LOW) s = "ON";
  mqtt.publish(MQTT_TOPIC"/relay_2/state", s.c_str());
}



void protocolParser() {

    // Unregistered or yet in progress
    if (id == 0) {
        mqtt.publish(MQTT_TOPIC"/node/debug", "protocolParser: ID = 0");
        if (Q_in[2] == 0xFE) print_msg(Q_in);

        // FE BF 02:got new client ID
        if (Q_in[2] == 0xFE && Q_in[4] == 0x02) {
            id = Q_in[5];
            if (id > 0x2F) id = 0x2F;
            SERUSB.println("Starting ID ack");  
            mqtt.publish(MQTT_TOPIC"/node/debug", "Starting ID ack");
            ID_ack(id);
            SERUSB.println("ID ack complete");  
            mqtt.publish(MQTT_TOPIC"/node/debug", "ID ack complete");
            mqtt.publish(MQTT_TOPIC"/node/id", String(id).c_str());
        }

        // FE BF 00:Any new clients?
        if (Q_in[2] == 0xFE && Q_in[4] == 0x00) {
            mqtt.publish(MQTT_TOPIC"/node/debug", "Sending ID Request");
            ID_request();
        }
    } else if (Q_in[2] == id && Q_in[4] == 0x06) { // we have an ID, do clever stuff
        if (send > 0) {
          char temp[64];
          sprintf(temp, "protocol: Start of the loop send = 0x%x", send);
          mqtt.publish(MQTT_TOPIC"/node/debug", temp);
        }

        // id BF 06:Ready to Send
        if (send == 0xff) {
            // 0xff marks dirty temperature for now
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x20);
            Q_out.push(settemp);
            {
              char temp[64];
              sprintf(temp, "protocol: Dirty Temperature setTemp = 0x%x", settemp);
              mqtt.publish(MQTT_TOPIC"/node/debug", temp);
            }
        } else if (send == 0xfe) {
            // 0xfe marks dirty time
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x21);
            Q_out.push(CurrentHours);
            Q_out.push(CurrentMins);
            {
              char temp[100];
              sprintf(temp, "protocol: Dirty Time CurrentHours = 0x%x, CurrentHours = 0x%x", CurrentHours, CurrentMins);
              mqtt.publish(MQTT_TOPIC"/node/debug", temp);
            }
        }else if (send == 0x00) {
            if (have_config == 0) { // Get configuration of the hot tub
                Q_out.push(id);
                Q_out.push(0xBF);
                Q_out.push(0x22);
                Q_out.push(0x00);
                Q_out.push(0x00);
                Q_out.push(0x01);
                //mqtt.publish(MQTT_TOPIC"/config/status", "Getting config");
                have_config = 1;
            } else if (have_faultlog == 0) { // Get the fault log
                Q_out.push(id);
                Q_out.push(0xBF);
                Q_out.push(0x22);
                Q_out.push(0x20);
                Q_out.push(0xFF);
                Q_out.push(0x00);
                have_faultlog = 1;
                mqtt.publish(MQTT_TOPIC"/node/msg", "Send fault log request");
            } else {
                // A Nothing to Send message is sent by a client immediately after a Clear to Send message if the client has no messages to send.
                Q_out.push(id);
                Q_out.push(0xBF);
                Q_out.push(0x07);
            }
        } else {
            // Send toggle commands
            {
              char temp[64];
              sprintf(temp, "protocol: Send toggle command = 0x%x", send);
              mqtt.publish(MQTT_TOPIC"/node/debug", temp);
            }
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x11);
            Q_out.push(send);
            Q_out.push(0x00);
        }

        rs485_send(false);
        send = 0x00;
    } else if (Q_in[2] == id && Q_in[4] == 0x2E) {
        if (last_state_crc != Q_in[Q_in[1]]) {
            decodeSettings();
        }
    } else if (Q_in[2] == id && Q_in[4] == 0x28) {
        mqtt.publish(MQTT_TOPIC"/node/msg", "Fault Log Received");
        if (last_state_crc != Q_in[Q_in[1]]) {
            decodeFault();
        }
    } else if (Q_in[2] == 0xFF && Q_in[4] == 0x13) { // FF AF 13:Status Update - Packet index offset 5
        if (last_state_crc != Q_in[Q_in[1]]) {
            decodeState();
        }
    } else {
        // DEBUG for finding meaning
        //if (Q_in[2] & 0xFE || Q_in[2] == id)
        //print_msg(Q_in);
    }

    // Clean up queue

    Q_in.clear();

}

