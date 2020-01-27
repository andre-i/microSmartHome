#ifndef SmsMaker_h
#define SmsMaker_h

#include <Arduino.h>
#include "Constants.h"
#include "Modem.h"

class SmsMaker{
  public: 
  SmsMaker(Modem *m);
  bool sendWarningSms(uint8_t clientOrder, int messageType);
  bool sendInfoSms();
  bool sendSms( char *smsFormat, char *phone, char *text);
  // check sms on admin number and fill chars array with sms content


  private:
  Modem *modem;
  void writeText(char *text);
  bool setSmsFormat(char *format);
  
};

#endif
