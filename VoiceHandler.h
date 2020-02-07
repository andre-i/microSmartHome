 #ifndef VoiceHandler_h
#define VoiceHandler_h

#include <Arduino.h>
#include "Constants.h"
#include "Modem.h"

class VoiceHandler{
  public:
    VoiceHandler(Modem *m);
    void setSensorsData(uint8_t minThemp, int themp, uint8_t motions);
    uint8_t handleIncoming(char *phone); // return SUCCESS if say answer and return ASSIGN_ADMIN if button pressed


private:
  //  var
    Modem *modem;
    int mode;
    uint8_t coolThemperature = 0;
    int themperature = 0;
    uint8_t motionCounter = 0;
    char call[30]; //  incoming calls
    char incomingPhone[14];
  // func
    uint8_t fillPhoneFromCall(char *str); 
    uint8_t makeAnswer(bool isAdmin);// for assign admin phone play only GOOD_SOUND
    uint8_t sayThemperature();
    uint8_t playFile(const char *fileName);
    void delayForPlay();
   // uint8_t 
};

#endif
