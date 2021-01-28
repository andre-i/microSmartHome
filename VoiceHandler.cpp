
#include "VoiceHandler.h"

extern volatile uint8_t delayEspired;

VoiceHandler::VoiceHandler(Modem *m) {
  modem = m;
}


void VoiceHandler::setSensorsData(uint8_t cool, int themp, uint8_t motions) {
  coolThemperature = cool;
  themperature = (themp > 0) ? themp : 1;
  motionCounter = motions;

}

/*
   call check on button pressed,
   if yes - fill phone with caller number
   and execute funk for play sound
   else - make answer as themperature and motion
*/
uint8_t VoiceHandler::handleIncoming(char *phone) {
#if DEBUG
  Serial.println(F("Handle voice call"));
#endif
  phone[0] = '\0';
  uint8_t ret;
  if (digitalRead(BUTTON_PIN) == HIGH)ret = makeAnswer(true);
  else {
    fillPhoneFromCall(phone);
    makeAnswer(false); // head func is assign admin phone
    ret = ASSIGN_ADMIN;
  }
  modem->dropGSM();
  return ret;
}



/*
    if success parse phone number return 'SUCCESS'
    othervice return 'WRONG_DATA'
*/
uint8_t VoiceHandler::fillPhoneFromCall(char *str) {
  setEspiredTime(DELAY_FOR_OK);
  while (modem->available() < 45 && delayEspired != 0)delay(2);
  if (modem->available() < 40)return WRONG_DATA;
  while (modem->read() != '"')delay(1);
  uint8_t i = 0;
  while (modem->available() > 0 && (str[i] = modem->read()) != '"' && i < 12)i++;
#if DEBUG
  Serial.print(F("  incoming = "));
  Serial.write(str, 12);
  Serial.println();
#endif
  if (i < 12) {
    str[0] = '\0';
    return WRONG_DATA;
  }
  str[12] = '\0';
  return SUCCESS;
}


/*
   make voice answer from module "amr" files
*/
uint8_t VoiceHandler::makeAnswer(bool isSayThemperature) {
  modem->sendCommand(F("ATA"));
  uint8_t retVal ;
  modem->sendCommand(F("AT+SIMTONE=1,1070,200,200,2200"));
  delay(2500);
  // if(!modem->checkOnOK(DELAY_FOR_OK))return NO_PLAY_SOUND;
  if (!isSayThemperature) {
    retVal = playFile(GOOD_SOUND);
    modem->sendCommand(F("ATH \r"));
    return retVal;
  }
  retVal = sayThemperature();
  if (motionCounter > 0) {
    if ( (retVal = playFile(MOTION_SOUND)) == SUCCESS) {
      const char fName[] = { motionCounter + 48, '.', 'a', 'm', 'r', '\0'};
      retVal = playFile(fName);
    }
  }
  if (digitalRead(VOLTAGE_PIN) == LOW) {
    retVal = playFile(NO_VOLTAGE);
   // for (uint8_t i = 0; i < 3; i++)playFile((retVal == SUCCESS) ? CAT_SOUND : BAD_SOUND);
  }
  modem->sendCommand(F("ATH \r"));
  return retVal;
}

/*
  uint8_t VoiceHandler::sayThemperature() {
  modem->dropGSM();
  if (playFile(VAWE_SOUND) != SUCCESS)return NO_PLAY_SOUND;
  if (playFile(FIRING_SOUND) != SUCCESS)return NO_PLAY_SOUND;
  String str;
  if (themperature < 21) {
    str = themperature + ".amr";
    if (playFile(str.c_str()) != SUCCESS)return NO_PLAY_SOUND;
  } else {
    uint8_t tens = 2;
    while ((themperature - tens * 10) > 9)tens++;
    str = String(tens) + "0.amr";
    if (playFile(str.c_str()) != SUCCESS)return NO_PLAY_SOUND;
    uint8_t ones = themperature - tens * 10;
    if (ones != 0) {
      str = String(ones) + ".amr";
      if ( playFile(str.c_str())  != SUCCESS)return NO_PLAY_SOUND;
    }
  }
  if (themperature < MIN_FIRING_THEMPERATURE)return playFile(BAD_SOUND);
  return  playFile(GOOD_SOUND);
  }
*/
uint8_t VoiceHandler::sayThemperature() {
  modem->dropGSM();
 // playFile(VAWE_SOUND);
  playFile(FIRING_SOUND);
  String str;
  if(themperature < 1) themperature = 0;
  Serial.println("  t = " + String(themperature));
  if (themperature < 21) {
    str = String(themperature) + ".amr";
    if (playFile(str.c_str()) != SUCCESS)return NO_PLAY_SOUND;
  } else {
    uint8_t tens = 2;
    while ((themperature - tens * 10) > 9)tens++;
    str = String(tens) + "0.amr";
    playFile(str.c_str());
    uint8_t ones = themperature - tens * 10;
    if (ones != 0) {
      str = String(ones) + ".amr";
      playFile(str.c_str());
    }
  }
  delayForPlay();
  if (themperature < coolThemperature)return playFile(BAD_SOUND);
  return  playFile(GOOD_SOUND);
}






/*
   play for caller sound from fileName
   return NO_PLAY_SOUND in any error
*/
uint8_t VoiceHandler::playFile(const char *fileName) {
//#if DEBUD
    Serial.print(" Try play file : ");
    Serial.println(fileName);
//#endif
  delay(10);
  while (modem->available() > 0) {
    modem->read();
    delay(1);
  }
  modem->print("AT+CPAMR=\"" + String(fileName) + "\",0 \r");
  if (!modem->checkOnOK(DELAY_FOR_OK)) {
    return NO_PLAY_SOUND;
  }
  delayForPlay();
  return SUCCESS;
}


void VoiceHandler::delayForPlay() {
  setEspiredTime(DELAY_FOR_SOUND);
  while ( modem->available() < 2 && delayEspired > 0){};
}
