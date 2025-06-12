/*
 * grove-can-bus.ino
 * Grove - CAN BUS Module based on GD32E103 for Wio BG770A
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#include <Adafruit_TinyUSB.h>
#include <WioCellular.h>

// CAN BUS Module settings
#define CAN_BAUDRATE_9600    9600
#define CAN_BAUDRATE_38400   38400
#define CAN_BAUDRATE_115200  115200

// CAN bus rates (based on Serial_CAN_Arduino library)
#define CAN_RATE_5      1
#define CAN_RATE_10     2
#define CAN_RATE_20     3
#define CAN_RATE_25     4
#define CAN_RATE_31_2   5
#define CAN_RATE_33     6
#define CAN_RATE_40     7
#define CAN_RATE_50     8
#define CAN_RATE_80     9
#define CAN_RATE_83_3   10
#define CAN_RATE_95     11
#define CAN_RATE_100    12
#define CAN_RATE_125    13
#define CAN_RATE_200    14
#define CAN_RATE_250    15
#define CAN_RATE_500    16
#define CAN_RATE_666    17
#define CAN_RATE_1000   18

// CAN Module class for Grove CAN BUS Module based on GD32E103
class WioCAN {
private:
  HardwareSerial* canSerial;
  char tempBuffer[100];
  
  void clearBuffer() {
    unsigned long timer_s = millis();
    while(1) {
      if(millis() - timer_s > 50) return;
      while(canSerial->available()) {
        canSerial->read();
        timer_s = millis();
      }
    }
  }
  
  bool sendCommand(const char* cmd) {
    unsigned long timer_s = millis();
    unsigned char len = 0;
    
    canSerial->println(cmd);
    while(1) {
      if(millis() - timer_s > 500) {
        return false;
      }
      
      while(canSerial->available()) {
        tempBuffer[len++] = canSerial->read();
        timer_s = millis();
      }
      
      if(len >= 4 && tempBuffer[len-1] == '\n' && tempBuffer[len-2] == '\r' && 
         tempBuffer[len-3] == 'K' && tempBuffer[len-4] == 'O') {
        clearBuffer();
        return true;
      }
    }
  }
  
  bool enterConfigMode() {
    canSerial->print("+++");
    clearBuffer();
    return true;
  }
  
  bool exitConfigMode() {
    clearBuffer();
    bool ret = sendCommand("AT+Q\r\n");
    clearBuffer();
    return ret;
  }

public:
  void begin(unsigned long baudrate = CAN_BAUDRATE_9600) {
    canSerial = &Serial1;
    canSerial->begin(baudrate);
    Serial.println("CAN Bus module initialized");
  }
  
  bool setCanRate(unsigned char rate) {
    enterConfigMode();
    if(rate < 10) {
      sprintf(tempBuffer, "AT+C=0%d\r\n", rate);
    } else {
      sprintf(tempBuffer, "AT+C=%d\r\n", rate);
    }
    
    bool ret = sendCommand(tempBuffer);
    exitConfigMode();
    return ret;
  }
  
  bool send(unsigned long id, unsigned char ext, unsigned char rtr, unsigned char len, const unsigned char* buf) {
    unsigned char data[14] = {0};
    
    data[0] = id >> 24;       // id3
    data[1] = (id >> 16) & 0xff; // id2
    data[2] = (id >> 8) & 0xff;  // id1
    data[3] = id & 0xff;      // id0
    
    data[4] = ext;
    data[5] = rtr;
    
    for(int i = 0; i < len; i++) {
      data[6 + i] = buf[i];
    }
    
    for(int i = 0; i < 14; i++) {
      canSerial->write(data[i]);
    }
    
    return true;
  }
  
  bool receive(unsigned long* id, unsigned char* buf) {
    if(!canSerial->available()) {
      return false;
    }
    
    unsigned long timer_s = millis();
    int len = 0;
    unsigned char data[20];
    
    while(1) {
      while(canSerial->available()) {
        data[len++] = canSerial->read();
        if(len == 12) break;
        
        if((millis() - timer_s) > 10) {
          canSerial->flush();
          return false;
        }
      }
      
      if(len == 12) {
        unsigned long receivedId = 0;
        
        for(int i = 0; i < 4; i++) {
          receivedId <<= 8;
          receivedId += data[i];
        }
        
        *id = receivedId;
        
        for(int i = 0; i < 8; i++) {
          buf[i] = data[i + 4];
        }
        return true;
      }
      
      if((millis() - timer_s) > 10) {
        canSerial->flush();
        return false;
      }
    }
  }
  
  void debugMode() {
    while(Serial.available()) {
      canSerial->write(Serial.read());
    }
    
    while(canSerial->available()) {
      Serial.write(canSerial->read());
    }
  }
};

WioCAN can;

void setup() {
  Serial.begin(115200);
  {
    const auto start = millis();
    while (!Serial && millis() - start < 5000) {
      delay(2);
    }
  }
  Serial.println();
  Serial.println("Grove CAN Bus Module for Wio BG770A");
  Serial.println("=====================================");

  WioCellular.begin();
  digitalWrite(PIN_VGROVE_ENABLE, VGROVE_ENABLE_ON);
  delay(100);  // Wait for Grove power to stabilize

  // Initialize CAN module
  can.begin(CAN_BAUDRATE_9600);
  
  // Set CAN bus rate to 500kbps (commonly used)
  if(can.setCanRate(CAN_RATE_500)) {
    Serial.println("CAN bus rate set to 500kbps: OK");
  } else {
    Serial.println("CAN bus rate set to 500kbps: FAILED");
  }
  
  Serial.println("Setup complete. Listening for CAN messages...");
}

void loop() {
  // Example: Send a test message every 5 seconds
  static unsigned long lastSend = 0;
  if(millis() - lastSend > 5000) {
    unsigned char testData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    can.send(0x123, 0, 0, 8, testData);
    Serial.println("Sent test CAN message (ID: 0x123)");
    lastSend = millis();
  }
  
  // Listen for incoming CAN messages
  unsigned long id = 0;
  unsigned char data[8];
  
  if(can.receive(&id, data)) {
    Serial.print("Received CAN message - ID: 0x");
    Serial.print(id, HEX);
    Serial.print(", Data: ");
    
    for(int i = 0; i < 8; i++) {
      Serial.print("0x");
      if(data[i] < 0x10) Serial.print("0");
      Serial.print(data[i], HEX);
      if(i < 7) Serial.print(" ");
    }
    Serial.println();
  }
  
  // Optional: Enable debug mode by sending commands through Serial monitor
  // Uncomment the following line to enable debug mode
  // can.debugMode();
}
