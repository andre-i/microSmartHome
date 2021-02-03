#include "HandlerHTTP.h"

extern volatile uint8_t delayEspired;

/*
    Api key must be set to sim phonebook as 5 record
    write command : at+cpbw=5,"12345",129,"#API_KEY#"
    ( head and tail # - is requered)
    For write add fields need remake:
        1) add fields to HandlerHTTP.h
        2) setSensorData implement set for add fields
        2) sendDataToServer add parameters and values for send(see it)

*/




HandlerHTTP::HandlerHTTP(Modem *m){
  modem = m;
}

void HandlerHTTP::init(){
 // Serial.println(F("Start init httpHandler"));
  modem->setFormatSms(GSM_FORMAT);
  isHaveParams = getRequestParam();
  modem->dropGSM();
 // Serial.println("\tEnd init http");
}

bool HandlerHTTP::isHaveParam(){
  return isHaveParams;
}


/*
 add to request header write api key and set fields list as chars array
*/
bool HandlerHTTP::setRequestParam(char *wKey /*, char *fields */){
  uint8_t n = 0;
  while(urlHead[n] != '='){
    requestHeader[n] = urlHead[n];
    n++;
  }
  requestHeader[n] = urlHead[n];
  n++;
  for(uint8_t i = 0; i < 16; i++)requestHeader[n + i] = wKey[i];
  requestHeader[n+17]='\0';
 // Serial.print(F("On get apiKey have request header : "));
  Serial.println(requestHeader);
  /* fieldsList = fields; */
  return true;
}

/*
* get params from phoneBook
   record 6 - read api key
    record 7 fields list ( not implemented)
*/
bool HandlerHTTP::getRequestParam(){
 // Serial.println(F("Start get request params"));
  modem->dropGSM();
  modem->sendCommand(getKeyCommand); // 6 is WRITE_API_KEY
  setEspiredTime(DELAY_FOR_ANSWER);
  char apiKey[17];
  char ch;
  // get api key
  while(modem->available() < 5 && delayEspired > 0)delay(5);
    delay(50);
    if(modem->available() < 35){
      modem-> dropGSM();
      return false;
  }
  uint8_t size = modem->available();
  ch = modem->read();
  while(modem->available() > 0 && modem->read() != '#')1;
  if(modem->available() < 17){
    modem->dropGSM();
    return false;
  }
  uint8_t i = 0;
  while( modem->available() > 0 && i < 16 && (ch = modem->read()) != '#' ){
    apiKey[i] = ch;
    i++;
  }
  apiKey[i] = '\0';
  //Serial.print(F("get key = "));
  //Serial.println(apiKey);
  modem->dropGSM();
  /* for set fields you must be implement it  see commened params bottom */
  return setRequestParam(apiKey /* , fields */);
}

/*
* set values for send to server
void HandlerHTTP::setSensorData(uint8_t themp, uint8_t  motCount){
    themperature = themp;
    motionCounter = motCount;
}
*/

/*
* Build modem commands for send data to server
* return codes may be [ SUCCESS 0, NO_SEND 1, WRONG_DATA 3 ]
* param:
*     themperature - themperature DS18B20
*     motionCounter - if last  DROP_MOTION_TIME(see in Constants) sensor occured motion - send number > 0
*     isHaveVoltage - 1 if outer voltage it is, 0 if battery power
*/
uint8_t HandlerHTTP::sendDataToServer(int themperature, uint8_t motionCounter, uint8_t isHaveVoltage){
 // Serial.println(F("Start send data to Server"));
  if(modem->openConnect()== false)return RETURN_ERROR;
 // Serial.println(F("Open connect"));
  modem->sendCommand(F("AT+HTTPPARA=\"CID\",1 "));
  if(!modem->checkOnOK(0))return NO_SEND;
  //char *command = "AT+HTTPPARA=\"URL\",\"";
  String head = "&field";
    // -------------------------------------------------------------------------------
    // on change methods(add fields) strings bottm must be remaked
    // ------------------------------------------------------------------------------
  String tail = "";
  tail += head + "1=" + String(themperature);
  tail += head + "2=" + String(motionCounter);
  tail += head + "3=" + isHaveVoltage + "\" ";
  String res = String(F("AT+HTTPPARA=\"URL\",\"")) + String(requestHeader) + String(tail);
  Serial.println(res);
  modem->sendCommand(res);
  if(!modem->checkOnOK(0)) {
  //  Serial.println(F("Error on set URL params"));
    return WRONG_DATA;
  }
  modem->sendCommand(F("AT+HTTPACTION=0"));
  uint8_t isSuccess = checkIsSuccess();
  modem->closeConnect();
  if(isSuccess != 0){
    modem->sendCommand(F("AT+SAPBR=0,1"));
    delay(200);
    modem->dropGSM();
    Serial.println(F("Drop Connect with ERROR\n"));
  }
  return isSuccess;
}

/*
* return 0 if return code == 200
*/
uint8_t HandlerHTTP::checkIsSuccess(){
  Serial.println(F("Start send"));
  setEspiredTime(DELAY_FOR_HTTP);
  while(modem->available() < 20 && delayEspired > 0){
    delay(50);
  }
  if(delayEspired == 0) return NO_SEND;// suspend
  delay(50);
  uint8_t size = modem->available();
  char answ[size+1];
  for(uint8_t i = 0; i < size; i++)answ[i]=modem->read();
  answ[size] = '\0';
  char pattern[] = { '2','0','0' };
 // Serial.print(F(" Get server answer : "));
  Serial.println(answ);
  modem->dropGSM();
  return strstr(answ, pattern)== NULL;
}
