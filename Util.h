#ifndef Util_h
#define Util_h


#include <Arduino.h>
#include "Constants.h"
#include "Modem.h"
#include <avr/wdt.h>

class Util {
  public:
    Util( Modem *m);
    uint8_t getTypeRequest(void);
    uint8_t handleIncomingSms(void);
    bool assignAdmin(char *phone);
    bool checkOnIncomingSms();
    uint8_t getCoolThemperature();
    uint8_t isSendMotionSMS();

  private:
    Modem *modem;
    uint8_t writeClientPhone(uint8_t clientNumber, char *phone);
    bool compareSenderWithAdmin(char *adminPhone);
    uint8_t executeRequest(void);
    void writeCharsArr(char *arr);
    uint8_t setCoolThemperature();
    uint8_t setIsMotion();
    void WDT_Reset();
};
#endif
