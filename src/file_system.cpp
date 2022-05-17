#include <Arduino.h>
#include "esp8266_spa.h"
#include "FS.h"
#include <SoftwareSerial.h>
#include <LittleFS.h>

extern SoftwareSerial swSer1;


bool readFile(const char* filename, char* value) {
  unsigned int i=0; 
  if (LittleFS.exists(filename)) {          // If the file exists
    File file = LittleFS.open(filename, "r"); // Open it
    if(!file || file.isDirectory()){
        SERUSB.println("- failed to open file for reading");
        return false;
    }
    while(file.available()) {
      value[i] = (unsigned char)file.read();
      i++;
    }
    value[i] = 0;  
    file.close();           // Then close the file again
    return true;
  }
    SERUSB.printf("\nFile not exist");
    return false;
}

void writeFile(const char* filename, const char* value) {
    File file = LittleFS.open(filename, "w"); // Open it
    if(!file){
        SERUSB.printf("%s - failed to open file for writing\n", filename);
        return;
    }
    if(file.print(value)!= strlen(value)){
        SERUSB.printf("%s - writing data \"%s\" failed\n", filename, value);
    }
    file.close();      // Then close the file again
}

void fsSetup(struct ConnectType *localConnect ) {
    if(!LittleFS.begin())  {                           // Start the SPI Flash Files System
        SERUSB.printf("\nError: Filesystem failed to mount");
        return;
      }
    if (!readFile("/ssid.txt", localConnect->ssid)) strcpy(localConnect->ssid, "your_ssid");
    if (!readFile("/password.txt", localConnect->password)) strcpy(localConnect->password, "your_passord");
    if (!readFile("/broker.txt", localConnect->brokerAddress)) strcpy(localConnect->brokerAddress, "your_broker");
    if (!readFile("/userid.txt", localConnect->brokerUserid)) strcpy(localConnect->brokerUserid, "");
    if (!readFile("/brokpass.txt", localConnect->brokerPassword)) strcpy(localConnect->brokerPassword, "");

    SERUSB.printf("\nSetup Data Read from file: SSID:%s Password:%s Broker:%s Broker UserID:%s Broker Password:%s\n", 
            localConnect->ssid, localConnect->password, localConnect->brokerAddress, localConnect->brokerUserid, localConnect->brokerPassword);
}
