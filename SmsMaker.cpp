

#include "SmsMaker.h"

extern volatile uint8_t delayEspired;

SmsMaker::SmsMaker(Modem *m) {
  modem = m;
}

/*
   send sms on cool firing , motion in house, droped external voltage
    messageType : COOL, MOTION, VOLTAGE
*/
bool SmsMaker::sendWarningSms(uint8_t clientOrder, int messageType) {
#if DEBUG
  Serial.println(F("sendWarningSms"));
#endif
  char mess[MAX_MESS_LEN];
  char phone[49];
  if (modem->fillUCS2PhoneNumber(clientOrder, phone) == false) {
#if DEBUG
    Serial.println(F(" err exit - Fail get phone"));
#endif
    return false;
  }
  phone[48] = '\0';
  strcpy_P(mess, (char *)pgm_read_word(&(messages[messageType])));
#if DEBUG
  /*
    uint8_t i = 0;
    Serial.print(F(" text msg: "));
    while (mess[i] != '\0') {
      Serial.write(mess[i]);
      i++;
    }
    Serial.println(F(" :"));
  */
#endif
  return sendSms( "UCS2\0", phone, mess );
}

/*
    it sms be send only admin
     it show clients and in phones
     Note: phones with 0 is empty phone
*/
bool SmsMaker::sendInfoSms() {
#if DEBUG
  Serial.println(F("\nsendInfoSms"));
#endif
  String answ = "";
  char phone[14];
  uint8_t j = 0;
  for (uint8_t i = 1; i <= COOL_THEMPERATURE_RECORD ; i++) {
    if (modem->fillGSMPhone(i, phone) == false) {
      Serial.println(F("\nError on fill phone"));
      // if clients not have phone - fill number of '0'
      for (uint8_t k = 0; k < 5; k++)phone[k] = '0';
      phone[5] = '\0';
      if ( i == 1)return false;
    }
    answ += '#';
    if (i == COOL_THEMPERATURE_RECORD)answ += 'T';
    else answ += i;
    answ += '#';
    while (phone[j] != '\0') {
      answ += phone[j];
      j++;
    }
    j = 0;
    answ += "#\n";
  }
  if (modem->fillGSMPhone(1, phone) == false)return false;
  return sendSms("GSM\0" , phone, (char*)answ.c_str());
}

/*
   @phone - international phone format( 13 chars starts with '+' and ends with '\0' )
              it must have appropriate format for GSM and Unicode sms
   @smsFormat "GSM\0" or "UCS2\0"
   @text must be ends with '\0' char
*/
bool SmsMaker::sendSms( char *smsFormat, char *phone, char *text) {
#if DEBUG
  //Serial.println(F(" Start write sms"));
  /*
    uint8_t i = 0;
    Serial.print(F("\n  sendSms [ format: "));
    Serial.print(smsFormat);
    Serial.print(F(" phone: "));
    while (phone[i] != '\0') {
      Serial.write(phone[i]);
      i++;
    }
    Serial.print("]\n text:\n");
    i=0;
    while(text[i] != '\0'){
      Serial.write(text[i]);
      i++;
    }
    Serial.println("\n\t_send success_\n");
    modem->dropGSM();
    return true;
  */
  if (digitalRead(DUCT_PIN) == LOW) {
    Serial.println(F("DEBUG mode emulator: set SMS true"));
    return true;
  }
#endif
  if (!setSmsFormat(smsFormat)) {
#if DEBUG
    // Serial.print(F(" err - can`t set format"));
#endif
    return false;
  }
  modem->print("OK \r");
  // REBOOT if any wrong
  if(!modem->checkOnOK(0)){
    modem->rebootSIM();
    Serial.println(F("WARNING: [ rebootSIM before send sms ]"));
  }
  modem->println("AT+CMGS=\"" + String(phone) + "\" \r");
  setEspiredTime(DELAY_FOR_OK);
  while (modem->available() < 6 && delayEspired != 0)delay(5);
  char ch = '\0';
  setEspiredTime(DELAY_FOR_OK);
  while (modem->available() > 0 && (ch = modem->read()) != '>' && delayEspired != 0) delay(2);
  modem->dropGSM();
  if (ch != '>') {
#if DEBUG
    Serial.println(F(" err: espired time for write text"));
#endif
    return false;
  }
  writeText(text);
  modem->write(0x1a);
  modem->write('\r');
  modem->flush();
#if DEBUG
  Serial.print(F("SMS send : "));
#endif
  setEspiredTime(DELAY_FOR_ANSWER);
  while (modem->available() < 3 && delayEspired != 0)delay(5);
  if (!modem->checkOnOK(0)) {
#if DEBUG
    Serial.println(F("'false'"));
#endif
    return false;
  }
#if DEBUG
  Serial.println(F("'true'"));
#endif
  return true;
}

/*
   tune modem for send sms appropriate format
*/
bool SmsMaker::setSmsFormat(char *format) {
  modem->dropGSM();
  if (strstr(format, "CS2\0") != NULL) {
    // ucs2 format
    modem->println(F("AT+CSMP=17,167,0,8  \r"));
    if (!modem->checkOnOK(2))return false;
    if (!modem->setFormatSms(UCS2_FORMAT))return false;
  } else if (strstr(format, "SM\0") != NULL) {
    // gsm format
    if (!modem->setFormatSms(GSM_FORMAT))return false;
  } else {
    Serial.println(F("\ntry set WRONG smsFormat"));
    return false;
  }
  return true;
}

/*
   write text as chars stream to modem input
*/
void SmsMaker::writeText(char *text) {
  char ch;
  uint8_t i = 0;
  while ( text[i] != '\0') {
#if DEBUG
    // Serial.write(text[i]);
#endif
    modem->write(text[i]);
    i++;
  }
}
