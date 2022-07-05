#include <CircularBuffer.h>

void RS485setup(void);
uint8_t crc8(CircularBuffer<uint8_t, 35> &data);
void ID_request();
void ID_ack(uint8_t lid);
void rs485_send(boolean loopback);
void spa_emulate_id();
void spa_emulate_request();
boolean spaReceive();
void RS485enableRx(void);