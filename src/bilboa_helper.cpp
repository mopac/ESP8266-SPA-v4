#include <Arduino.h>
#include "esp8266_spa.h"

extern CircularBuffer<uint8_t, 35> Q_in;
extern CircularBuffer<uint8_t, 35> Q_out;
extern PubSubClient mqtt;
extern void _yield();

void RS485setup(void) {
  // Begin RS485 in listening mode
  #ifndef AUTO_TX

    pinMode(TX485_RE, OUTPUT);
    digitalWrite(TX485_RE, LOW);
    pinMode(TX485_DE, OUTPUT);
    digitalWrite(TX485_DE, LOW);
  
  #endif

  pinMode(RLY1, OUTPUT);
  digitalWrite(RLY1, HIGH);
  pinMode(RLY2, OUTPUT);
  digitalWrite(RLY2, HIGH);

}

uint8_t crc8(CircularBuffer<uint8_t, 35> &data) {
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


void spa_emulate() {
  Q_out.push(0xFE);
  Q_out.push(0xBF);
  Q_out.push(0x02);
  Q_out.push(0x00);

  rs485_send(true);
}

boolean spaReceive() {
    uint8_t x=0;
    volatile uint8_t counter = 0;
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