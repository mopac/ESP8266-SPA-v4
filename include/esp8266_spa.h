
#define TX485_RE D5  //Receiver Enable
#define TX485_DE D6  //Driver Enable
#define RLY1  D1
#define RLY2  D2

#define SWAP
#ifdef SWAP
  #define SERUSB swSer1
  #define SER485 Serial
#else
  #define SERUSB Serial
  #define SER485 swSer1
#endif

//HomeAssistant autodiscover
// #define HASSIO

// #define AUTO_TX //if your RS485 driver chip needs enables Tx automatically then uncomment this #define 
#define VERSION "0.40"

// Define the top level topic used for all MQTT messages
#define MQTT_TOPIC "Spa"

#define STRON String("ON").c_str()
#define STROFF String("OFF").c_str()

#include <CircularBuffer.h>
#include <PubSubClient.h>       // MQTT client

struct SpaStateType{
  uint8_t jet1 :1;
  uint8_t jet2 :1;
  uint8_t blower :1;
  uint8_t light :1;
  uint8_t restmode:1;
  uint8_t highrange:1;
  uint8_t opmode :2;
  uint8_t hour :5;
  uint8_t minutes :6;
};

struct SpaConfigType{
  uint8_t pump1 :2; //this could be 1=1 speed; 2=2 speeds
  uint8_t pump2 :2;
  uint8_t pump3 :2;
  uint8_t pump4 :2;
  uint8_t pump5 :2;
  uint8_t pump6 :2;
  uint8_t light1 :1;
  uint8_t light2 :1;
  uint8_t circ :1;
  uint8_t blower :1;
  uint8_t mister :1;
  uint8_t aux1 :1;
  uint8_t aux2 :1;
};

struct SpaFaultLogType{
  uint8_t totEntry :5;
  uint8_t currEntry :5;
  uint8_t faultCode :6;
  String faultMessage;
  uint8_t daysAgo :8;
  uint8_t hour :5;
  uint8_t minutes :6;
};

struct ConnectType{
  char ssid[20];
  char password[20];
  char brokerAddress[20];
  char brokerUserid[20];
  char brokerPassword[20];
};