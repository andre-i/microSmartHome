#include "Modem.h"

extern volatile uint8_t delayEspired;


Modem::Modem(SoftwareSerial *my) {
  gsm = my;
  gsm->begin(9600);
}

void Modem::changeShowCommand(uint8_t isShow) {
  isShowCommand = (isShow) ? true : false;
  Serial.print(F("\n\tvisibility command = "));
  Serial.println((isShow) ? "true" : "false");
}

void Modem::initModem() {
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(RST_PIN, SIM_NO_RST);
  // start SIM900
  setEspiredTime(250);
  char ch;
  while (gsm->available() > 0  || delayEspired != 0) {
    if (gsm->available() > 0) {
      ch = gsm->read();
#if DEBUG
      //   Serial.write(ch);
#endif
    }
    if (ch == 'y')break;
    delay(1);
  }
  if ( delayEspired == 0 ) {
    // ------ if can`t get "call ready" answer then must make the reboot modem(SIM900) -------
    if (isShowCommand)Serial.println(F("\n\tERROR [ can`t start SIM900 - try it reboot]\n"));
    digitalWrite(RST_PIN, SIM_RST);
    setEspiredTime(250);
    while (delayEspired != 0) {}
    rebootSIM();
  }
  else {
    // ---------------------- on success get connect tune modem ---------------------------
    if (isShowCommand) Serial.println(" \nEnds of wait ");
    dropGSM();
    sendCommand(F("ATE0 \r")); // echo off
    sendCommand(F("ATV1 \r")); // out on echo off
    sendCommand(F("AT+CMGF=1 \r")); // sms text mode
    sendCommand(F("AT+CSCS=\"gsm\" \r")); // sms gsm charset
    sendCommand(F("AT+CPBS=\"SM\" \r")); // set "SM" phonebook
    sendCommand(F("AT+CLIP=1 \r")); // The calling line identity(auto get caller phone number
    sendCommand(F("AT+CFSINIT \r")); //
    delay(2000);
    dropGSM();
    delay(2000);
    digitalWrite(LED_PIN, LOW);
    Serial.println(F("\n\tsuccess Init\n"));
    if (gsm->available() > 0) dropGSM();
  }
}




/*
   after check answer from SIM flush RX
   buffer(this help avoid some missmath)
*/
void Modem::dropGSM() {
  delay(20);
  setEspiredTime(2);
  while ( gsm->available() < 1 && delayEspired > 0) {
    delay(2);
  }
  while (gsm->available() > 0) {
    read();
    delay(2);
  }
}


/*
    send command to SIM as complete String
*/
void Modem::sendCommand(String toSIM) {
  dropGSM();
#if DEBUG
  if (isShowCommand) {
    Serial.print(F("\nsend command: "));
    Serial.print(toSIM);
    Serial.print("  ");
  }
#endif
  gsm->println(toSIM);
  gsm->flush();
  delay(1);
}



/*
    stop httpService and close gprs connect
*/
bool Modem::closeConnect(void) {
#if DEBUG
  Serial.println(F("\ncloseConnect"));
#endif
  sendCommand(F("AT+HTTPTERM \r"));
  //  checkOnOK(DELAY_FOR_ANSWER);
  dropGSM();
  sendCommand(F("AT+SAPBR=0,1 \r"));
  if (!checkOnOK(DELAY_FOR_ANSWER)) {
#if DEBUG
    //  Serial.println(F("Fail close gprs call \"check on suspend\""));
#endif
    sendCommand("AT \r");
    if (!checkOnOK(0))rebootSIM();
  }
  dropGSM();
  return true;
}

/*
    check answer on contain "Ok" or "ERROR"
    return true if contain "OK"
*/
bool Modem::checkOnOK(uint8_t wait_time) {
  delay(10);
  char ch = '\0';
  bool checkNext = false;
#if DEBUG
  if (isShowCommand) Serial.print(F(" CheckOnOK   "));
#endif
  if (wait_time == 0)setEspiredTime(DELAY_FOR_OK);
  else setEspiredTime(wait_time);
  while ( gsm-> available() < 4 && delayEspired > 0) {
    delay(1);
  }
  if (gsm->available() < 2)return false;
  while ( gsm->available() > 0 ) {
    ch =  gsm->read();
    if ( ch == '\r' || ch == '\n' || ch == ' ') {
      delay(1);
      checkNext = false;
      continue;
    }
#if DEBUG
    if (isShowCommand)Serial.write(ch);
#endif
    if ( checkNext && (ch == 'K')) {
#if DEBUG
      if (isShowCommand)  Serial.println(F("  : true"));
#endif
      dropGSM();
      delayEspired = 0;
      return true;
    }
    if (ch == 'O')checkNext = true;
    else checkNext = false;
    if (ch == 'R') {
      dropGSM();
      delayEspired = 0;
#if DEBUG
      if (isShowCommand) Serial.println(F("  : false"));
#endif
      return false;
    }
    delay(1);
  }
#if DEBUG
  if (isShowCommand) Serial.println(F("  : false"));
#endif
  dropGSM();
  return false;
}



void Modem::rebootSIM(void) {
#if DEBUG
  Serial.println(F("\nREBOOT"));
#endif
  digitalWrite(RST_PIN, SIM_RST);
  delay(9000);
  initModem();
}


/*
   fill client number on order in phonebook
*/
bool Modem::fillUCS2PhoneNumber(uint8_t clientNumber, char *phone) {
  dropGSM();
  if (setFormatSms(UCS2_FORMAT) == false)return false;
  String command = String("AT+CPBR=") + clientNumber + " \r ";
#if DEBUG
  /*
    Serial.print(F("fillPhoneNumber(UCS2) for "));
    Serial.print(clientNumber);
    Serial.println("  command " + command + "\n");
    dropGSM();
    sendCommand(command);
    delay(1);
    // wait for answer and start parse gotten
    setEspiredTime(DELAY_FOR_OK);
    while(available()<30 && delayEspired != 0)delay(5);
    while(available()>0)Serial.write(read());
    Serial.println();
  */
#endif
  dropGSM();
  sendCommand(command);
  char ch = '\0';
  setEspiredTime(DELAY_FOR_OK);
  while (available() < 30 && delayEspired != 0)delay(1);
  uint8_t i = 0;
  delay(20);
  // wait for answer and start parse gotten
  setEspiredTime(DELAY_FOR_OK);
  while (available() > 0 && delayEspired != 0) {
    if (available() > 0 && i < 3) {
      if (read() == '"') i++;
    }
    if ( i == 3 ) break;
    delay(1);
  }
  setEspiredTime(DELAY_FOR_OK);
  while (available() < 48 && delayEspired != 0)delay(2);
  // too small data -> wrong send
  if (available() <  48) {
#if DEBUG
    Serial.print(F(" [short or can`t get answer on reques phone: "));
    while (available() > 0)Serial.write(read());
    Serial.println(" ]");
#endif
    phone[0] = '\0';
    dropGSM();
    return false;
  }
  // if (debug)Serial.print(F(" start parse:  "));
  uint8_t has_exit = 0;
  i = 0;
  while ( available() > 0 && !has_exit) {
    ch = read();
    // if (debug)Serial.write(ch);
    if (ch == '"') {
      has_exit = 1;
      continue;
    }
    phone[i] = ch;
    i++;
    delay(1);
  }
  dropGSM();
  /* if (debug) {
     Serial.println();
     Serial.println(" Phone contain " + String(i) + " Symbols ");
    }
  */
  if (i < (UCS2_PHONE_SIZE - 1)) {
    phone[0] = '\0';
    return false;
  }
  return true;
}

/*
   fill cool themoperature from sim phonebook
*/
bool Modem::fillMinThemperature(char *T) {
  return fillGSMPhone(COOL_THEMPERATURE_RECORD, T);
}

/*
 * return whether send motion warning sms
 */
uint8_t Modem::isSendMotion(){
  char  field[13];
  if(fillGSMPhone(IS_SEND_MOTION_RECORD, field)){
    if(field[0] == 'y')return SEND_SMS;
  }
  return NO_SEND_SMS;
}


/*

*/
bool Modem::fillGSMPhone(uint8_t clientNumber, char *phone) {
#if DEBUG
  /*
    Serial.print(F("Start Fill phone number(GSM) client: "));
    Serial.print(clientNumber);
    Serial.write(' ');
  */
#endif
  phone[0] = '\0';
  dropGSM();
  if (!setFormatSms(GSM_FORMAT))return false;
  String command = String("AT+CPBR=") + clientNumber + " \r ";
  dropGSM();
  sendCommand(command);
  setEspiredTime(2);
  while (available() < 31 && delayEspired != 0) delay(1);
  delay(5);
  if (available() < 30 ) {
    Serial.println(F(" Error read phone - Wery short answer"));
    return false;
  }
  while (read() != '"' && available() > 0)delay(1);
  uint8_t i = 0;
  while (available() > 0 && i < 13 ) {
    phone[i] = read();
    if (phone[i] == '"') {
      phone[i] = '\0';
      break;
    }
    i++;
  }
  if (clientNumber > CLIENT2)return true;
#if DEBUG
  // for ( uint8_t j = 0; j < i; j++)Serial.write(phone[j]);
  // Serial.write(' ');
#endif
  if (i < 12) {
    Serial.print(F(" Error read phone - Short phone number book record ="));
    Serial.println(clientNumber);
    phone[0] = '\0';
    return false;
  }
  if (checkPhoneFormat(phone) == false) {
    phone[0] = '\0';
    return false;
  }
  Serial.println();
  return true;
}

/*
    check phone number in GSM  format
*/
bool Modem::checkPhoneFormat(char *phone) {
  bool retVal = true;
  if (phone[0] != '+')retVal = false;
  for (uint8_t i = 1; i < 12; i++) {
    if ( phone[i] < 48 || phone[i] > 57 ) retVal = false;
  }
  if (retVal == false)Serial.println(F(" Wrong format phone number!"));
  return retVal;
}

bool Modem::setFormatSms( const char *format) {
#if DEBUG
  Serial.print(F(" setFormatSms : "));
#endif
  char answ[16];
  uint8_t i = 0;
  println("AT \r");
  checkOnOK(0);
  sendCommand(F("AT+CSCS? \r"));
  setEspiredTime(DELAY_FOR_OK);
  while (available() < 15 && delayEspired != 0)delay(1);
  delay(10);
  if (available() < 15) return false;
  while (available() > 0 && i < 15) {
    answ[i] = read();
    i++;
  }
  answ[i] = '\0';
#if DEBUG
  /*
    Serial.print(F(" answ : "));
    Serial.print(answ);
    Serial.print(" - ");
  */
#endif
  if (strstr(answ , format) != NULL) {
#if DEBUG
    Serial.println(F("Not change "));
#endif
    return true;
  }
  sendCommand("AT+CSCS=\"" + String(format) + "\" \r");
#if DEBUG
  Serial.println(F("changed "));
#endif
  return checkOnOK(DELAY_FOR_OK);
}#include "Modem.h"

extern volatile uint8_t delayEspired;


Modem::Modem(SoftwareSerial *my) {
  gsm = my;
  gsm->begin(9600);
}

void Modem::changeShowCommand(bool isShow) {
  isShowCommand = isShow;
  Serial.print(F("\n\tvisibility command = "));
  Serial.println((isShow) ? "true" : "false");
}

void Modem::initModem() {
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(RST_PIN, SIM_NO_RST);
  // start SIM900
  setEspiredTime(250);
  char ch;
  while (gsm->available() > 0  || delayEspired != 0) {
    if (gsm->available() > 0) {
      ch = gsm->read();
#if DEBUG
      //   Serial.write(ch);
#endif
    }
    if (ch == 'y')break;
    delay(1);
  }
  if ( delayEspired == 0 ) {
    // ------ if can`t get "call ready" answer then must make the reboot modem(SIM900) -------
    if (isShowCommand)Serial.println(F("\n\tERROR [ can`t start SIM900 - try it reboot]\n"));
    digitalWrite(RST_PIN, SIM_RST);
    setEspiredTime(250);
    while (delayEspired != 0) {}
    rebootSIM();
  }
  else {
    // ---------------------- on success get connect tune modem ---------------------------
    if (isShowCommand) Serial.println(" \nEnds of wait ");
    dropGSM();
    sendCommand(F("ATE0 \r")); // echo off
    sendCommand(F("ATV1 \r")); // out on echo off
    sendCommand(F("AT+CMGF=1 \r")); // sms text mode
    sendCommand(F("AT+CSCS=\"gsm\" \r")); // sms gsm charset
    sendCommand(F("AT+CPBS=\"SM\" \r")); // set "SM" phonebook
    sendCommand(F("AT+CLIP=1 \r")); // The calling line identity(auto get caller phone number
    sendCommand(F("AT+CFSINIT \r")); //
    delay(2000);
    dropGSM();
    delay(2000);
    digitalWrite(LED_PIN, LOW);
    Serial.println(F("\n\tsuccess Init\n"));
    if (gsm->available() > 0) dropGSM();
  }
}




/*
   after check answer from SIM flush RX
   buffer(this help avoid some missmath)
*/
void Modem::dropGSM() {
  delay(20);
  setEspiredTime(2);
  while ( gsm->available() < 1 && delayEspired > 0) {
    delay(2);
  }
  while (gsm->available() > 0) {
    read();
    delay(2);
  }
}


/*
    send command to SIM as complete String
*/
void Modem::sendCommand(String toSIM) {
  dropGSM();
#if DEBUG
  if (isShowCommand) {
    Serial.print(F("\nsend command: "));
    Serial.print(toSIM);
    Serial.print("  ");
  }
#endif
  gsm->println(toSIM);
  gsm->flush();
  delay(1);
}



/*
    stop httpService and close gprs connect
*/
bool Modem::closeConnect(void) {
#if DEBUG
  Serial.println(F("\ncloseConnect"));
#endif
  sendCommand(F("AT+HTTPTERM \r"));
  //  checkOnOK(DELAY_FOR_ANSWER);
  dropGSM();
  sendCommand(F("AT+SAPBR=0,1 \r"));
  if (!checkOnOK(DELAY_FOR_ANSWER)) {
#if DEBUG
    //  Serial.println(F("Fail close gprs call \"check on suspend\""));
#endif
    sendCommand("AT \r");
    if (!checkOnOK(0))rebootSIM();
  }
  dropGSM();
  return true;
}

/*
    check answer on contain "Ok" or "ERROR"
    return true if contain "OK"
*/
bool Modem::checkOnOK(uint8_t wait_time) {
  delay(10);
  char ch = '\0';
  bool checkNext = false;
#if DEBUG
  if (isShowCommand) Serial.print(F(" CheckOnOK   "));
#endif
  if (wait_time == 0)setEspiredTime(DELAY_FOR_OK);
  else setEspiredTime(wait_time);
  while ( gsm-> available() < 4 && delayEspired > 0) {
    delay(1);
  }
  if (gsm->available() < 2)return false;
  while ( gsm->available() > 0 ) {
    ch =  gsm->read();
    if ( ch == '\r' || ch == '\n' || ch == ' ') {
      delay(1);
      checkNext = false;
      continue;
    }
#if DEBUG
    if (isShowCommand)Serial.write(ch);
#endif
    if ( checkNext && (ch == 'K')) {
#if DEBUG
      if (isShowCommand)  Serial.println(F("  : true"));
#endif
      dropGSM();
      delayEspired = 0;
      return true;
    }
    if (ch == 'O')checkNext = true;
    else checkNext = false;
    if (ch == 'R') {
      dropGSM();
      delayEspired = 0;
#if DEBUG
      if (isShowCommand) Serial.println(F("  : false"));
#endif
      return false;
    }
    delay(1);
  }
#if DEBUG
  if (isShowCommand) Serial.println(F("  : false"));
#endif
  dropGSM();
  return false;
}



void Modem::rebootSIM(void) {
#if DEBUG
  Serial.println(F("\nREBOOT"));
#endif
  digitalWrite(RST_PIN, SIM_RST);
  delay(9000);
  initModem();
}


/*
   fill client number on order in phonebook
*/
bool Modem::fillUCS2PhoneNumber(uint8_t clientNumber, char *phone) {
  dropGSM();
  if (setFormatSms(UCS2_FORMAT) == false)return false;
  String command = String("AT+CPBR=") + clientNumber + " \r ";
#if DEBUG
  /*
    Serial.print(F("fillPhoneNumber(UCS2) for "));
    Serial.print(clientNumber);
    Serial.println("  command " + command + "\n");
    dropGSM();
    sendCommand(command);
    delay(1);
    // wait for answer and start parse gotten
    setEspiredTime(DELAY_FOR_OK);
    while(available()<30 && delayEspired != 0)delay(5);
    while(available()>0)Serial.write(read());
    Serial.println();
  */
#endif
  dropGSM();
  sendCommand(command);
  char ch = '\0';
  setEspiredTime(DELAY_FOR_OK);
  while (available() < 30 && delayEspired != 0)delay(1);
  uint8_t i = 0;
  delay(20);
  // wait for answer and start parse gotten
  setEspiredTime(DELAY_FOR_OK);
  while (available() > 0 && delayEspired != 0) {
    if (available() > 0 && i < 3) {
      if (read() == '"') i++;
    }
    if ( i == 3 ) break;
    delay(1);
  }
  setEspiredTime(DELAY_FOR_OK);
  while (available() < 48 && delayEspired != 0)delay(2);
  // too small data -> wrong send
  if (available() <  48) {
#if DEBUG
    Serial.print(F(" [short or can`t get answer on reques phone: "));
    while (available() > 0)Serial.write(read());
    Serial.println(" ]");
#endif
    phone[0] = '\0';
    dropGSM();
    return false;
  }
  // if (debug)Serial.print(F(" start parse:  "));
  uint8_t has_exit = 0;
  i = 0;
  while ( available() > 0 && !has_exit) {
    ch = read();
    // if (debug)Serial.write(ch);
    if (ch == '"') {
      has_exit = 1;
      continue;
    }
    phone[i] = ch;
    i++;
    delay(1);
  }
  dropGSM();
  /* if (debug) {
     Serial.println();
     Serial.println(" Phone contain " + String(i) + " Symbols ");
    }
  */
  if (i < (UCS2_PHONE_SIZE - 1)) {
    phone[0] = '\0';
    return false;
  }
  return true;
}

/*
   fill cool themoperature from sim phonebook
*/
bool Modem::fillMinThemperature(char *T) {
  return fillGSMPhone(COOL_THEMPERATURE_RECORD, T);
}



/*

*/
bool Modem::fillGSMPhone(uint8_t clientNumber, char *phone) {
#if DEBUG
  /*
    Serial.print(F("Start Fill phone number(GSM) client: "));
    Serial.print(clientNumber);
    Serial.write(' ');
  */
#endif
  phone[0] = '\0';
  dropGSM();
  if (!setFormatSms(GSM_FORMAT))return false;
  String command = String("AT+CPBR=") + clientNumber + " \r ";
  dropGSM();
  sendCommand(command);
  setEspiredTime(2);
  while (available() < 31 && delayEspired != 0) delay(1);
  delay(5);
  if (available() < 30 ) {
    Serial.println(F(" Error read phone - Wery short answer"));
    return false;
  }
  while (read() != '"' && available() > 0)delay(1);
  uint8_t i = 0;
  while (available() > 0 && i < 13 ) {
    phone[i] = read();
    if (phone[i] == '"') {
      phone[i] = '\0';
      break;
    }
    i++;
  }
  if (clientNumber == COOL_THEMPERATURE_RECORD)return true;
#if DEBUG
  // for ( uint8_t j = 0; j < i; j++)Serial.write(phone[j]);
  // Serial.write(' ');
#endif
  if (i < 12) {
    Serial.print(F(" Error read phone - Short phone number book record ="));
    Serial.println(clientNumber);
    phone[0] = '\0';
    return false;
  }
  if (checkPhoneFormat(phone) == false) {
    phone[0] = '\0';
    return false;
  }
  Serial.println();
  return true;
}

/*
    check phone number in GSM  format
*/
bool Modem::checkPhoneFormat(char *phone) {
  bool retVal = true;
  if (phone[0] != '+')retVal = false;
  for (uint8_t i = 1; i < 12; i++) {
    if ( phone[i] < 48 || phone[i] > 57 ) retVal = false;
  }
  if (retVal == false)Serial.println(F(" Wrong format phone number!"));
  return retVal;
}

bool Modem::setFormatSms( const char *format) {
#if DEBUG
  Serial.print(F(" setFormatSms : "));
#endif
  char answ[16];
  uint8_t i = 0;
  println("AT \r");
  checkOnOK(0);
  sendCommand(F("AT+CSCS? \r"));
  setEspiredTime(DELAY_FOR_OK);
  while (available() < 15 && delayEspired != 0)delay(1);
  delay(10);
  if (available() < 15) return false;
  while (available() > 0 && i < 15) {
    answ[i] = read();
    i++;
  }
  answ[i] = '\0';
#if DEBUG
  /*
    Serial.print(F(" answ : "));
    Serial.print(answ);
    Serial.print(" - ");
  */
#endif
  if (strstr(answ , format) != NULL) {
#if DEBUG
    Serial.println(F("Not change "));
#endif
    return true;
  }
  sendCommand("AT+CSCS=\"" + String(format) + "\" \r");
#if DEBUG
  Serial.println(F("changed "));
#endif
  return checkOnOK(DELAY_FOR_OK);
}
