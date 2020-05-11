#ifndef Modem_h
#define Modem_h


#include <Arduino.h>
#include <SoftwareSerial.h>
#include "Constants.h"


//
class Modem {
  public:
  Modem(SoftwareSerial *s);
    // STREAM HANDLERS
    void initModem();
    void dropGSM();
    void sendCommand(String toSIM);
    void rebootSIM(void);
    bool closeConnect(void);
    // wrapped methods from SoftvareSerial
    size_t write(uint8_t byte) {
      return gsm->write(byte);
    };
    int read() {
      return gsm->read();
    };
    int available() {
      return gsm->available();
    };
    void flush() {
      gsm->flush();
      delay(10);
    };
    int print(String str) {
      int len =  gsm->print(str);
      gsm-> flush();
      delay(5);
      return len; 
    };
    int println(String str) {
      return print( str + '\n');
    };
    bool checkOnOK(uint8_t wait_time);
    // utility
    bool setFormatSms( const char *format);
    bool fillUCS2PhoneNumber(uint8_t numberInBook, char *phone);
    bool fillGSMPhone(uint8_t clientNumber, char *phone);
    bool checkPhoneFormat(char *phone); // phone number must be in GSM format
    void changeShowCommand(uint8_t isShow); // whether show command on it call
    bool fillMinThemperature(char *phone);
    uint8_t isSendMotion();
/* ==================== PRIVATE ====================*/
  private:
    // var
    SoftwareSerial *gsm;
    bool isShowCommand = false;
    // func
};



#endif
