void handleRoot();
void handleLoopback();
void handleSetup();
void handleNotFound();
String getAPName();
void mqttpubsub();
void reconnect();
//void callback(char* p_topic, byte * p_payload, unsigned int p_length);

#if defined(ARDUINO_ARCH_ESP32)
    #define WL_MAC_ADDR_LENGTH 6
#endif