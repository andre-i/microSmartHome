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
   // uint8_t setApiKey(char *apiKey);
   // uint8_t setFieldsList( char *fieldsList);

  private:
    Modem *modem;
    uint8_t writeToPhoneBook(uint8_t clientNumber, char *phone);
    bool compareSenderWithAdmin(char *adminPhone);
    uint8_t executeRequest(void);
    void writeCharsArr(char *arr);
    uint8_t setCoolThemperature();
    uint8_t setIsMotion();
    uint8_t startReboot();
    void WDT_Reset();
};
#endif
