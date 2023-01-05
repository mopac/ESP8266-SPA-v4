#include <Arduino.h>
#include "esp8266_spa.h"
#include <SoftwareSerial.h>

extern CircularBuffer<uint8_t, 40> Q_in;
extern CircularBuffer<uint8_t, 40> Q_out;
extern PubSubClient mqtt;
extern void _yield();
extern SoftwareSerial swSer1;

void RS485setup(void) {
  // Begin RS485 in listening mode
  #ifndef AUTO_TX

    pinMode(TX485_RE, OUTPUT);
    digitalWrite(TX485_RE, HIGH); // Receiver disabled
    pinMode(TX485_DE, OUTPUT);
    digitalWrite(TX485_DE, LOW); // Transmitter disabled
    SERUSB.println("Using TX485_RE & TX485_DE");

  
  #endif

  pinMode(LED, OUTPUT);    // Set up the LED output
  digitalWrite(LED, LED_OFF); // Turn the LED off
  pinMode(RLY1, OUTPUT);
  digitalWrite(RLY1, HIGH);
  pinMode(RLY2, OUTPUT);
  digitalWrite(RLY2, HIGH);

}

void RS485enableRx(void) {
  digitalWrite(TX485_RE, LOW); // Receiver enabled
}

uint8_t crc8(CircularBuffer<uint8_t, 40> &data) {
  unsigned long crc;
  int i, bit;
  uint8_t length = data.size();

  crc = 0x02;
  for ( i = 0 ; i < length ; i++ ) {
    crc ^= data[i];
    for ( bit = 0 ; bit < 8 ; bit++ ) {
      if ( (crc & 0x80) != 0 ) {
        crc <<= 1;
        crc ^= 0x7;
      }
      else {
        crc <<= 1;
      }
    }
  }

  return crc ^ 0x02;
}

void rs485_send(boolean loopback) {

    digitalWrite(LED, LED_ON); // Turn the LED on

  // The following is not required for the new RS485 chip
  #ifndef AUTO_TX

    if(!loopback) digitalWrite(TX485_RE, HIGH);
    digitalWrite(TX485_DE, HIGH);
    delay(1);

  #endif


  // Add telegram length
  Q_out.unshift(Q_out.size() + 2);

  // Add CRC
  Q_out.push(crc8(Q_out));

  // Wrap telegram in SOF/EOF
  Q_out.unshift(0x7E);
  Q_out.push(0x7E);

  for (int i = 0; i < Q_out.size(); i++)
    SER485.write(Q_out[i]);

  //print_msg(Q_out);

  SER485.flush();

  #ifndef AUTO_TX
  
    digitalWrite(TX485_RE, LOW);
    digitalWrite(TX485_DE, LOW);
  
  #endif

  // DEBUG: print_msg(Q_out);
  Q_out.clear();
  digitalWrite(LED, LED_OFF); // Turn the LED off

}


void ID_request() {
  Q_out.push(0xFE);
  Q_out.push(0xBF);
  Q_out.push(0x01);
  Q_out.push(0x02);
  Q_out.push(0xF1);
  Q_out.push(0x73);

  rs485_send(false);
}

void ID_ack(uint8_t lid) {
  Q_out.push(lid);
  Q_out.push(0xBF);
  Q_out.push(0x03);

  rs485_send(false);
}


void spa_emulate_id() {
  Q_out.push(0xFE);
  Q_out.push(0xBF);
  Q_out.push(0x02);
  Q_out.push(0x00); // ID

  rs485_send(true);
}

void spa_emulate_request() {
  Q_out.push(0x17);
  Q_out.push(0xBF);
  Q_out.push(0x06);
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 
  Q_out.push(0x00); 

  rs485_send(true);
}


void spaEmulateStatus() {
// This is the message the tub sends periodically with its current status
// 7e 20 ff af 13 00 00 3c c 02 00 00 01 03 01 00 00 00 00 00 00 00 00 00 00 3c 00 00 00 78 00 00 e6 7e 
  Q_out.push(0xFF);
  Q_out.push(0xAF);
  Q_out.push(0x13);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x3c);
  Q_out.push(0x0C);
  Q_out.push(0x02);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x01);
  Q_out.push(0x03);
  Q_out.push(0x01);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x3C);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x00);
  Q_out.push(0x78);
  Q_out.push(0x00);
  Q_out.push(0x00);

  rs485_send(true);
}




boolean spaReceive() {
    uint8_t x=0;
    static uint8_t counter = 0;
    extern uint64_t lastrx;
    boolean messageComplete = false;
    // Read from Spa RS485
    if (SER485.available()) {
      // SERUSB.println("Read Serial");  
      x = SER485.read();
      Q_in.push(x);

      // Drop until SOF is seen
      if (Q_in.first() != 0x7E) Q_in.clear();
      lastrx = millis();
    // Turned on just for debug
      if (counter < 100 ) {
        String s;
        if (x < 0x0A) s += "0";
        s += String(x, HEX);
        mqtt.publish(MQTT_TOPIC"/rcv", s.c_str());
        _yield();
        counter++;
      }
      // Double SOF-marker, drop last one
      if (x == 0x7E && Q_in.size() > 2) {
        messageComplete = true;
        if (Q_in[1] == 0x7E) Q_in.pop();
      }
    }
    return messageComplete;
}