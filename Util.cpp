#include "Util.h"

extern volatile uint8_t delayEspired;

Util::Util( Modem *m) {
  modem = m;
}

/*
   compute type incoming req by first symbols
*/
uint8_t Util::getTypeRequest(void) {
  setEspiredTime(DELAY_FOR_ANSWER);
  while (modem->available() < 14 && delayEspired != 0)delay(2);
  if (modem->available() < 14)return UNDEFINED;
  char req[13];
  for (uint8_t i = 0; i < 12; i++) {
    req[i] = modem->read();
  }
  req[12] = '\0';
  if ( strstr( req, "RING") != NULL)return INC_CALL;
  if (strstr( req, "+CMTI") != NULL)return INC_SMS;
  Serial.print(F("\n\tWARNING: undefined modem MSG\n get from modem [ "));
  Serial.print(req);
  while (modem->available() > 0)Serial.write(modem->read());
  Serial.println(" ]\n");
  return UNDEFINED;
}

/*
   assign phone as admin phone and clear client1 and client2 phones
*/
bool Util::assignAdmin(char *phone) {
  if (!modem->checkPhoneFormat(phone))return false;
#if DEBUG
  Serial.print(F("assignAdmin: phone right "));
#endif
  ;
  if (!modem->setFormatSms(GSM_FORMAT)) return false;
  if (writeToPhoneBook( ADMIN, phone) != SUCCESS)return false;
#if DEBUG
  Serial.print(F("success write admin "));
#endif
  char empty[13] = { '+', '0', '0', '0', '\0' };
  if (writeToPhoneBook( CLIENT1, empty) != SUCCESS) {
    Serial.print(F("wrong erase client1"));
  }
  if (writeToPhoneBook( CLIENT2, empty) != SUCCESS) {
    Serial.print(F("wrong erase client2"));
  }
  return SUCCESS;
}

uint8_t Util::writeToPhoneBook(uint8_t clientNumber, char *str) {
  uint8_t ret = WRONG_DATA;
#if DEBUG
  uint8_t i = 0;
  Serial.print(F("\n try write [ "));
  Serial.print(String(clientNumber) + " ; ");
  while (str[i] != '\0') {
    Serial.write(str[i]);
    i++;
  }
  Serial.print(" ] ");
  ret = SUCCESS;
#endif
    modem->dropGSM();
  if (modem->setFormatSms(GSM_FORMAT)) {
    delay(20);
    modem->dropGSM();
    modem->print("AT+CPBW=" + String(clientNumber) + ",\"");
    if(clientNumber != WRITE_API_KEY )writeCharsArr(str);
    else {
      char arr[] = {'1','2','3','4','5'};
      writeCharsArr(arr);
    }
    if (clientNumber < COOL_THEMPERATURE_RECORD) {
      modem->print("\",145,\"");
      writeCharsArr(str);
      modem->println("\" \r");
    }
    else {
      if ( clientNumber == COOL_THEMPERATURE_RECORD) modem->println("\",129,\"coolThemp\" \r");
      if ( clientNumber == IS_SEND_MOTION_RECORD)modem->println("\",129,\"sendMotion\" \r");
      if ( clientNumber == WRITE_API_KEY) modem->println(String("\",129,\"#") +  String(str) + String("#\" \r"));
    //  if ( clientNumber == FIELDS_LIST) modem->println("\",129,\"#123#\" \r");
    }
    modem->flush();
    delay(20);
    if (modem->checkOnOK(0)) ret =  SUCCESS;
    else ret = WRONG_TASK;
  }
#if DEBUG
  Serial.println(" return "+ String(ret));
#endif
  return ret;
}

void Util::writeCharsArr(char *arr) {
  uint8_t i = 0;
  while (arr[i] != '\0') {
    modem->write(arr[i]);
    i++;
    delay(1);
  }
}

/*
   check first sms on
                      1. empty
                      2. sender is admin
   if success check then call function for handle request
*/
uint8_t Util::handleIncomingSms() {
  uint8_t ret;
  Serial.print(F("handleIncomingSms "));
  char admin[13];
  if (!modem->fillGSMPhone( ADMIN, admin))return RETURN_ERROR;
  modem->dropGSM();
  if (!modem->setFormatSms(GSM_FORMAT))return RETURN_ERROR;
  modem->sendCommand(F("AT+CMGR=1 \r"));
  setEspiredTime(DELAY_FOR_OK);
  while (modem->available() < 40 && delayEspired > 0)delay(2);
  if (modem->available() > 35) {
    if (compareSenderWithAdmin(admin)) {
      ret = executeRequest();
    }
    else ret =  WRONG_USER;
  } else {
    if (digitalRead(DUCT_PIN) == LOW) {
      Serial.print(F("DEBUG: it is not SMS - content("));
      while (modem->available() > 0)Serial.write(modem->read());
    }
    Serial.println(" )");
    
    ret = IS_EMPTY;
  }
  modem->dropGSM();
  if (ret != IS_EMPTY)modem->sendCommand(F("AT+CMGDA=\"DEL ALL\" \r"));
  delay(1);
  modem->dropGSM();
  return ret;
}

/*
  get sender number from sms adn call function for compare
  with admin number and read all symbols to new line symbols(include)
  return success if equals
*/
bool Util::compareSenderWithAdmin(char *adminPhone) {
#if DEBUG
  Serial.print(F(" Start compare with admin phone: "));
#endif
  while (modem->read() != ',')delay(1);
  modem->read(); // read first '"' char
  uint8_t i = 0;
  char ch;
  while (modem->available() > 0 && i < 12) {
    ch = modem->read();
#if DEBUG
    Serial.write(ch);
#endif
    if ( adminPhone[i] != ch) return false;
    i++;
  }
  // drop chars while not end string
  while (modem->available() > 0 && modem->read() != '\n')delay(1);
  return true;
}

/*
   check on right request format
   if info request @return SEND_INFO_SMS
   otherwise read clientNumber and it phone
   after call functions for handle write clients phone
*/
uint8_t Util::executeRequest(void) {
  char ch;
  uint8_t clientNumber;
  Serial.print(F("  Execute incoming command "));
  while ( modem->available() > 0 && (ch = modem->read()) != '#') {
    delay(1);
    Serial.write(ch);
  }
  Serial.println();
  if (modem->available() == 0)return WRONG_TASK;
  ch = modem->read();
  if (ch == 'T' || ch == 't') {
    uint8_t ret = setCoolThemperature();
    if ( ret != SET_COOL_THEMPERATURE_RECORD) {
      Serial.print(F("\n  ERROR - can`t set cool themperature CODE = "));
      Serial.println(ret);
    }
    WDT_Reset(); // RESET ON SUCCESS write cool themperature
    return ret;
  }
  if ( ch == 'M' || ch == 'm')return setIsMotion();
  if(ch == 'R' || ch =='r') return startReboot();
  if (ch != 'w' && ch != 'W') return SEND_INFO_SMS;
  if (modem->read() != '#')return WRONG_TASK;
  if (modem->available() > 0)ch = modem->read();
  if ( ch != '1' && ch != '2' && ch != '3')return WRONG_TASK;
  clientNumber = ch - 48;
  //  permitted write phones for 1, 2 and 3 record in phonebook
  if (modem->available() > 0 && modem->read() != '#')return WRONG_TASK;
  char phone[13];
  uint8_t i;
  for (i = 0; i < 12; i++) {
    if (modem->available() > 0) phone[i] = modem->read();
    else break;
  }
  if ( i < 11)return WRONG_DATA;
  phone[12] = '\0';
  if (!modem->checkPhoneFormat(phone)) return WRONG_DATA;
  return writeToPhoneBook( clientNumber, phone);
}

bool Util::checkOnIncomingSms() {
  modem->setFormatSms(GSM_FORMAT);
  modem->dropGSM();
  modem->sendCommand(F("AT+CMGR=1 \r"));
  setEspiredTime(DELAY_FOR_OK);
  while (modem->available() < 40 && delayEspired != 0)delay(1);
  return modem->available() > 35;
}

uint8_t Util::setIsMotion() {
#if DEBUG
  Serial.print(F(" wtite isMotion "));
#endif
  char ch;
  char isSend[2];
  isSend[1] = '\0';
  if( modem->read() != '#'){
    Serial.println(F("fail send motion"));
    return WRONG_DATA;
  }
  ch = modem->read();
  if ( ch == 'Y' || ch == 'y' )isSend[0] = '1';
  else  isSend[0] = '0';
  if ( writeToPhoneBook(IS_SEND_MOTION_RECORD, isSend) != SUCCESS)return RETURN_ERROR;
  WDT_Reset();
  return SUCCESS;
}

uint8_t Util::isSendMotionSMS() {
  return modem->isSendMotion();
}


/*
    2 methods for  work with cool themperature
*/

uint8_t Util::setCoolThemperature() {
#if DEBUG
  Serial.print(F(" wtite cool_themperature "));
#endif
  char themp[3];
  themp[2] = '\0';
  char ch;
  if (modem->read() != '#')return WRONG_TASK;
  ch = modem->read();
  if ( ch < 48 || ch > 57)  return WRONG_DATA; // not digits chsr
  themp[0] = ch;
  ch = modem->read();
  if ( ch == '#') themp[1] = '\0';
  else {
    if ( ch < 48 && ch > 57) return WRONG_DATA;
    themp[1] = ch;
  }
  if (writeToPhoneBook(COOL_THEMPERATURE_RECORD, themp) != SUCCESS)return NO_SEND;
  WDT_Reset();
#if DEBUG
  Serial.print(F("SUCCESS t = "));
  Serial.println(themp);
#endif
  return SET_COOL_THEMPERATURE_RECORD;
}


uint8_t Util::getCoolThemperature() {
#if DEBUG
  Serial.println(F("  getCoolThemp"));
#endif
  char themp[13];
  uint8_t T;
  modem->fillMinThemperature(themp);
  if (themp[0] < 48 || themp [0] > 57 ||
      ( themp[1] != '\0' && (themp[1] < 48 || themp[1] > 57)))return MIN_FIRING_THEMPERATURE;
  if (themp[1] == '\0') T = themp[0] - 48;
  else T = (themp[0] - 48) * 10 + (themp[1] - 48);
  if ( T < 1 || T > 99)return MIN_FIRING_THEMPERATURE;
  return T;
}

/*
 * reserved for ...
uint8_t Util::setApiKey(char *apiKey){
  return writeToPhoneBook(WRITE_API_KEY, apiKey);
}

uint8_t Util::setFieldsList(char *fieldsList){
  return writeToPhoneBook(FIELDS_LIST, fieldsList);
}
*/

/*
 * start reboot on admin demand
 * sms command must be : #r(R)#o(O)#d(D) - (r)eboot (o)n (d)emand
 */
uint8_t Util::startReboot(){
  delay(100);
  char ch;
  if(modem->available() < 4) return WRONG_TASK;
  if(modem->read() == '#'){
    ch = modem->read();
    if( (ch == 'o' || ch == 'O') && modem->read() == '#') {
      ch = modem->read();
      if(ch == 'd' || ch == 'D'){
        Serial.println(F("Reset on ADMIN demand"));
        WDT_Reset();
        return SUCCESS;
      }
    }
  }else{
    return WRONG_TASK;
  }  
  
}

/*
    reset Arduino after set new values for
   1) cool themperature or
   2) change send motion sms state
*/
void Util::WDT_Reset() {
  digitalWrite(RST_PIN, LOW);
  delay(9000);
  wdt_enable(WDTO_8S);//  REBOOT the device after 8 seconds
  Serial.println(F("\n\n\t RESET ON CHANGE PARAMETERS \n"));
  while(Serial.available() > 0)Serial.read();
  modem->dropGSM();
  delay(9000);
}
