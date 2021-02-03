#ifndef HandlerHTTP_h
#define HandlerHTTP_h

#define DELAY_FOR_HTTP 30

#include <Arduino.h>
#include "Modem.h"
#include "Constants.h"

class HandlerHTTP {

public:
  HandlerHTTP(Modem *m);
  bool isHaveParam();
  // void setSensorData(uint8_t themperature, uint8_t  motionCounter);
  uint8_t sendDataToServer(int themperature, uint8_t  motionCounter,uint8_t voltage);
  void init();
private:
  Modem *modem;
  //  function
  bool setRequestParam(char *wKey /* , char *fields */);
  bool getRequestParam();
  uint8_t checkIsSuccess();
  // fields
  char  urlHead[44] = "http://api.thingspeak.com/update?api_key=";
  char getKeyCommand[10] = "AT+CPBR=6";
  int themperature = 0;
  uint8_t motionCounter = 0;
  uint8_t voltage = 0;
  char requestHeader[64];
  //char *fieldsList;
  bool isHaveParams = false;

};






#endif
