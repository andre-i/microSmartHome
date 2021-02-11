#define VERSION 0.09
/*
            changes:
       
        1) add reset on admin demand( get appropriate sms )
        2) add support for send data to ThingSpeak
 */


/*
     PIN description
     D2(2) - motion sensor
     D3(3) - RX (SoftwareSerial)
     D4(4) - TX (SoftwareSerial)
     D5(5) - button pin
     D6(6) - duct pin
     D7(7) - DDS18B20 pin( ONE_WIRE )

     D9(9) - reset for SIM900

*/

/*
  System work with 3 clients maximum. First client is admin - it have rights on change
  phones clients
    admin SMS pattern
  1) change client phone -  #w#num#..phone..#
        num - client order may be 1, 2, 3 ( 1- admin)
  2) set min themperature for cool sms - #t#themp#
        themp - need themperature from 1 to 99(only number)
  3) call send Info sms - #i# i may be replaced any symbols not w or t
      on this sms module be sent sms with clients phones
     format answers: #1#...admin-phone...#
                     #2#..client2-phone..#
                     #3#..client3-phone..#
                     #T=...#
                     #Motion mode : (SEND or NO_SEND) #
   phone - International number type (starts with '+' and have 11 digits)
   client 2 and client3 can`t mandatory
   T=...  - themperature for send cool firing sms
  4) for whether send motion warning sms - #My# or #Mn#
              #M#y# - motion sms be send
              #M#n# - motion sms no send
  Warning : after set new values the arduino module make reset
  5) reset module - #R#O#D  - (letters may be capitalise or not)
                    r - reset, 0 - on, d - demand
-----------  set parameter from DUCT mode  -------------------------------------
 all comands must be set only in "GSM" mode - AT+CSCS="GSM"
1) clients(1-3) AT+CPBW=(1-3),"(PHONE)",145, "(PHONE)"
                (1-3) client number from 1 to 3
                PHONE tel number +.....(11 digits)
2) set cool tehemperature format AT+CPBW=4,t,129,"coolTemp"
3) set is send motionSms format AT+CPBW=5,(0 or 1),129,"isSendMotion"
                                          1 - send, 0 - not send
4) set write api key for ThingSpeak AT+CPBW=6,"12345",129,"#apiKey#"
                                    #apiKey# - char '#' is mandatory
                                               apiKey - 16 symbols from ThingSpeak(as showed)

*/

// Include the libraries we need
#include <avr/wdt.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include "timer-api.h"
/* projects files */
#include  "Modem.h"
#include "SmsMaker.h"
#include "VoiceHandler.h"
#include "Constants.h"
#include "Util.h"
#include "HandlerHTTP.h"

// redifene max buffer for SoftwareSerial otherwice
//  can`t set request (small buffer size)
#define _SS_MAX_RX_BUFF 128

/*  -- THEMPERATURE  --  */
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
DeviceAddress sensorAddr;
/* ---  SoftwareSerial  ---   */
SoftwareSerial pipe(RX_PIN, TX_PIN);

/*  CLASSES  */
Modem modem(&pipe);
SmsMaker sms(&modem);
VoiceHandler voice(&modem);
Util util(&modem);
HandlerHTTP http(&modem);

/*  variables  */

// flags
volatile uint8_t flags;
#define CHECK_FLAG          0       // flag on check sensors values
#define RESET_FLAG          1       //  reset system flag
#define     Free_flag_2         2
#define     Free_flag_3         3
#define     Free_flag_4         4
#define MOTION_SEND_FLAG    5       //set after send  sms on motion in house
#define COOL_SEND_FLAG      6       //set after send  sms on very small firing themperature
#define VOLTAGE_SEND_FLAG   7       //set after send  sms on drop external power
volatile uint8_t warningFlags;
// if flag set need send Warning sms
#define COOL_SMS            0
#define MOTION_SMS          1
#define VOLTAGE_SMS         2

// external intr handler
uint8_t volatile motionCounter = 0;
unsigned volatile int dropMotionTimer = 0;
// timer intr handler
uint8_t volatile delayEspired = 0;

// temperature
uint8_t coolThemperature = MIN_FIRING_THEMPERATURE;

// send SMS on motion
uint8_t isSendMotion = NO_SEND_SMS;

// work MODE
//  work normally - worked device(DUCT_PIN not pressed)
//  duct - pipe beetwen serial and modem
//  test - no send sms
enum modes { DUCT = 0, TEST = 1, WORK = 2};
uint8_t mode = WORK;



void setup() {
  wdt_reset();
  wdt_disable();
  // start up serial port`s
  Serial.begin(9600);
  Serial.println(" ");
  Serial.print(F("\n\tStart_Smart_Home_Apps v_"));
  Serial.println(VERSION);
  // SIM
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, SIM_RST);
  delay(1000);
  // led
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  //motion sensor
  pinMode(MOTION_SENSOR_PIN, INPUT);
  digitalWrite(MOTION_SENSOR_PIN, LOW);
  //  check outer voltage
#if READ_AS_ANALOG == 0
  pinMode(VOLTAGE_PIN, INPUT);
#endif
  /*  ISR   */
  // timer 2 intr handler freq=1Hz, period=1s
  timer_init_ISR_1Hz(TIMER_DEFAULT);
  //  start up the motion sensor handler - external intr
  attachInterrupt(digitalPinToInterrupt(MOTION_SENSOR_PIN), motion_sensor_INTR_handler, RISING);

  // Start up the Dallas library
  sensors.begin();
  // get address for connected sensors
  if (!sensors.getAddress(sensorAddr, 0)) Serial.println(F("\nDallas Lib -Unable to find address for Device 0"));
  /*
       // search for devices on the bus and assign based on an index.
    if (!sensors.getAddress(sensorAddr, 0)) Serial.println(F("Unable to find address for Device 0"));
    else {
    Serial.print("\n\tDevice address = ");
    printAddress(sensorAddr);
    }
  */
  delay(2000);
  Serial.println(F("\n  START WATCH !!!\n"));
  modem.initModem();  // init sim module
  motionCounter = 0;
  bitClear(warningFlags, MOTION_SMS);
  flags = 0b11100000;
  // init params
  isSendMotion = util.isSendMotionSMS();
  coolThemperature = util.getCoolThemperature();
  Serial.println("\nOn Init coolThemperature= " + String(coolThemperature) +  "  isSendMotion= " + String(isSendMotion));
  // prepare SIM900 module(clear all sms)
  modem.dropGSM();
  modem.sendCommand(F("AT+CMGDA=\"DEL ALL\" \r"));
  delay(500);
  modem.dropGSM();
  // init for ThingSpeak service
  http.init();
  executeScheduledTask();
}

void loop() {

  // ------- DUCT for swap beetwen serial and modem -----------
  if (digitalRead(DUCT_PIN) == LOW && mode == DUCT) {
    while (pipe.available() > 0)Serial.write(pipe.read());
    while (Serial.available() > 0)pipe.write(Serial.read());
  } else {

    // --------------- warnings SMS ---------------------
    // voltage
#if READ_AS_ANALOG == 0    
    if (digitalRead(VOLTAGE_PIN) == LOW
        && bitRead(flags, VOLTAGE_SEND_FLAG) == 1) {
      Serial.println(F("Start send low VOLTAGE sms"));
      if (sendWarning(VOLTAGE))bitClear(flags, VOLTAGE_SEND_FLAG);
    }
#endif
    // motion
    if (bitRead(warningFlags, MOTION_SMS) == 1
        && bitRead(flags, MOTION_SEND_FLAG) == 1 && isSendMotion == SEND_SMS ) {
      Serial.println(F("Start send motion warning sms"));
      if (sendWarning(MOTION)) bitClear(flags, MOTION_SEND_FLAG);
    }
    //  cool firing
    if (bitRead(warningFlags, COOL_SMS) == 1
        && bitRead(flags, COOL_SEND_FLAG) == 1) {
      Serial.println(F("Start send cool warning sms"));
      if (sendWarning(COOL))bitClear(flags, COOL_SEND_FLAG);
    }

    // ---------------  check themperature  -----------------
    if (bitRead(flags, CHECK_FLAG) == 1)executeScheduledTask();

    // ------------  LISTEN MODEM  ----------------------
    if (modem.available() > 0) handleModemMsg();

    // after wait time motion counter be dropped and drop send motion sms
    if (motionCounter == 0 && bitRead(warningFlags, MOTION_SMS) == 1) {
      bitClear(warningFlags, MOTION_SMS);
    }
  }
  // flip beetwen DUCT and no sms modes
  if (digitalRead(BUTTON_PIN) == LOW && digitalRead(DUCT_PIN)  == LOW)buttonHandle();
}


/*  ====================================================
    ============== Util section ========================
    ====================================================
*/
/*  DS18B20 ADDRESS  */
/*
  // function to print a device address
  void printAddress(DeviceAddress deviceAddress)
  {
  Serial.print("{ ");
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print(deviceAddress[i]);
    Serial.print(":");
  }
  Serial.print(" }\n");
  }
*/

/*
   on click Button change mode and
   reflect current mode
   DUCT - arduino work as pipe beetwen modem amd computer
   TEST - no sms send mode( device check sensors and execute all actions
            as normally work exclude sms send). Also in this mode groowe
            debug messages
*/
void buttonHandle() {
  delay(20);
  int pressCounter = 0;
  for (uint8_t i = 0; i < 6; i++) {
    if (digitalRead(BUTTON_PIN) == LOW)pressCounter++;
    else pressCounter--;
    delay(20);
  }
  if (pressCounter > 2 ) {
    Serial.print("\n ___ mode : ");
    switch (mode) {
      case DUCT: mode = TEST; Serial.print("TEST");
        break;
      case TEST: mode = WORK ; Serial.print("WORK");
        break;
      case WORK: mode = DUCT; Serial.print("DUCT");
      default:
        Serial.println(" ___");
    }
    modem.changeShowCommand(mode);
  }
  while (digitalRead(BUTTON_PIN) == LOW) {}
}

/*
   Read themperature and set it to VoiceHandler
   if themperature small than MIN_FIRING_THEMPERATURE
    call func for send warning sms
*/
void executeScheduledTask() {
  modem.sendCommand(F("AT+GSMBUSY=1 \r"));  // lock modem
  if (digitalRead(DUCT_PIN) == HIGH)digitalWrite(LED_PIN, LOW);// off led
#if DEBUG
 // Serial.print(F("\n\t readThemperature "));
#endif
  modem.dropGSM();
  modem.print("AT \r");
  if (!modem.checkOnOK(0)) {
    modem.rebootSIM();
#if DEBUG
    Serial.print(F("\tModem can`t set answer - start REBOOT"));
#endif
  }
  bitClear(flags, CHECK_FLAG);
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures
  int themperature = (int)sensors.getTempC(sensorAddr);
  // set new data
  voice.setSensorsData(coolThemperature, themperature, motionCounter);
  //http.setSensorData(themperature, motionCounter);
  if (digitalRead(DUCT_PIN) == LOW)Serial.println("\n\t t = " + String(themperature) + "; motion = " + String(motionCounter) + " ;");
  if (themperature < coolThemperature && bitRead(flags, COOL_SEND_FLAG) == 1) {
    Serial.println(F("\tCall for send cool SMS"));
    bitSet(warningFlags, COOL_SMS);
  }
  if (themperature > coolThemperature && bitRead(warningFlags, COOL_SMS) == 1)bitClear(warningFlags, COOL_SMS);
  // after check on warning - send data to server
  if(http.isHaveParam()){
    int res = http.sendDataToServer(themperature, motionCounter,
      #if READ_AS_ANALOG==0
        (digitalRead(VOLTAGE_PIN) == HIGH));
      #else
        (analogRead(VOLTAGE_PIN));
      #endif
    Serial.println("On send return code: " + String(res));
  }
  handleSms(); // in this function execute AT+GSMBUSY=0 - modem unlock
}

/*
    send warnings sms
    types is COOL=0, MOTION=1, VOLTAGE=2
*/
bool sendWarning( uint8_t warningType ) {
  modem.sendCommand(F("AT+GSMBUSY=1 \r"));
  if (!modem.checkOnOK(0)) {
    // REBOOT on any wrong
#if DEBUG
    Serial.println(F("Reboot SIM on start send SMS"));
#endif
    modem.rebootSIM();
    modem.sendCommand(F("AT+GSMBUSY=1 \r"));
  }
  modem.dropGSM();
  bool res = false;
  if (warningType != VOLTAGE) {
    if (sms.sendWarningSms(CLIENT2, warningType)) res = true;
    if (sms.sendWarningSms(CLIENT1, warningType)) res = true;
  }
  if (res == false)res = sms.sendWarningSms( ADMIN, warningType);
  modem.sendCommand(F("AT+GSMBUSY=0 \r"));
  modem.dropGSM();
  Serial.println(" send sms result - " + String(res));
  return res;
}


/*
   handle emited of  the modem out
*/
void handleModemMsg() {
  Serial.println(F(" handleModemMSG"));
  uint8_t code, ret;
  char phone[13];
  ret = util.getTypeRequest();// compute type request from modem output
  switch (ret) {
    case INC_SMS:
      code = handleSms();
      break;
    case INC_CALL:
      code = voice.handleIncoming(phone);
      if (code == ASSIGN_ADMIN)break;
      handleSms(); break;
    case UNDEFINED: 
       code = IS_EMPTY;
       modem.dropGSM(); break;
  }
#if DEBUG
  Serial.print("  handleModemMsg CODE : " + String(code));
#endif
  if ( code == ASSIGN_ADMIN ) {
    if (util.assignAdmin(phone))sms.sendInfoSms();
  }
  else if ( code == IS_EMPTY)Serial.println(F(" handleModemMsg : Modem OUT can`t sms or incoming call"));
  modem.dropGSM();
}

/*
    handle sms if it is
*/
uint8_t handleSms() {
  modem.sendCommand(F("AT+GSMBUSY=1 \r"));// send busy for all caller
  modem.checkOnOK(0);
  modem.dropGSM();
  uint8_t code = util.handleIncomingSms();
  if (code == SEND_INFO_SMS) sms.sendInfoSms();
  if (code == SET_COOL_THEMPERATURE_RECORD)coolThemperature = util.getCoolThemperature();
  modem.sendCommand(F("AT+GSMBUSY=0 \r"));// ready make answers for caller
  modem.checkOnOK(0);
  modem.dropGSM();
  return code;
}



/* ==============================================
                    INTERRUPTS
   ==============================================
*/

/*
  ISR handler for interrupt maked from motion sensor signals
  after wait time motion be set init state in timer handler
*/
void motion_sensor_INTR_handler(void) {
  dropMotionTimer = DROP_MOTION_TIME;   // start wait for drop motion counter
  if (bitRead(flags, MOTION_SEND_FLAG) == 1 )bitSet(warningFlags, MOTION_SMS);
  if (motionCounter < 10) motionCounter++;
}

/*
    timer handler
    execute some shedule tasks
*/
void timer_handle_interrupts(int timer) {
  //   inner variables
  static uint16_t timerCounter;
  static uint8_t loopCounter;

  //  measure time for check themperature
  static uint16_t checkSensors;
  //  for set flags leaves send messages
  static uint16_t coolEspired;
  static uint16_t motionEspired;
  static uint16_t voltageEspired;

  // delayEspired
  // wait delay for answer from modem
  if (delayEspired > 0 ) {
    delayEspired--;
  }

  //  check sensors values
  checkSensors++;
  if (checkSensors == CHECK_SENSOR_TICK) {
    bitSet(flags, CHECK_FLAG);
    checkSensors = 0;
  }

  //  motion espired
  //  run motion sensor set espired time > 0
  // if espired time is 1 drop motion counter
  if (dropMotionTimer > 0) dropMotionTimer--;
  else motionCounter = 0;

  //    ==========  SEND SMS  ================
  //  work with delay time beetwen send cool sms
  if (bitRead(flags, COOL_SEND_FLAG) == 0) {
    coolEspired ++;
    if (coolEspired == DELAY_FOR_COOL) {
      bitSet(flags, COOL_SEND_FLAG);
      coolEspired = 0;
    }
  }
  //  work with delay time beetwen send motion sms
  if (bitRead(flags, MOTION_SEND_FLAG) == 0) {
    motionEspired ++;
    if (motionEspired == DELAY_FOR_MOTION) {
      bitSet(flags, MOTION_SEND_FLAG);
      motionEspired = 0;
    }
  }
  //  work with delay time beetwen send drop voltage sms
  if (bitRead(flags, VOLTAGE_SEND_FLAG) == 0) {
    voltageEspired ++;
    if (voltageEspired == DELAY_FOR_VOLTAGE) {
      bitSet(flags, VOLTAGE_SEND_FLAG);
      voltageEspired = 0;
    }
  }

  // blink on DUCT
  if (digitalRead(DUCT_PIN) == LOW) digitalWrite( LED_PIN, !digitalRead(LED_PIN));

  //  WDT soft reset after 4 timerCounter loop
  //  device make restart after ~ 2 days work
  timerCounter ++;
#if DEBUG == 0
  if (timerCounter == 43200) {
#else
  if (timerCounter == 300) {
#endif
    loopCounter++;
    timerCounter = 0;
    if (loopCounter == 4) {
      digitalWrite(RST_PIN, LOW);
      delay(9000);
      //  REBOOT the device after 2 days work
      wdt_enable (WDTO_8S);
      Serial.println(F("\n___________________________\n\tAFTER 8 seconds to be RESET\n___________________________"));
      delay(9000);

    }
  }
#if DEBUG
  //  WARNING - this action is only debug and may be leave to brokem programm work
  // it work wery randomly
  // purpose this chunk - fast call sms handler
  // if(timerCounter == 20) handleSms();
#endif
}


//  ============================================================================
//                        GLOBAL FUNCTIONS
//  ============================================================================



/*
   Set delay time for any GSM task
*/
void setEspiredTime(uint8_t delayTime) {
  noInterrupts();
  delayEspired = delayTime;
  interrupts();
}

/*
   if src contain pattern return 1
   otherwise return 0
*/
/*
int findInStr( char *src, char *pattern){
   // printf("On check src=%s pattern=%s", src,pattern);
    int n=0;
    while(src[n] != '\0' && n < 200){
        while( src[n] != '\0' && src[n] != pattern[0])n++;
        if(src[n] == '\0') return 0;
        n = checkInSubstr(src, pattern, n);
        if( n == 0) return 1;
    }
   // printf("\n End of str");
    return 0;
}

/*
  add-on for findInStr function
  compare first part of tail src string by pattern
  if equals return 0
  otherwise return start position of remainder src(unchecked tail)
*/
/*
int checkInSubstr(char *src, char *pattern, int srcPos){
    srcPos++;  //(first symb. is equals)
    int m = 1;  //  second symbol in pattern
    while(pattern[m] != '\0' && src[srcPos] != '\0'){
      // if symbols not equals - return unchecked tail position
        if(pattern[m] != src[srcPos] || src[srcPos] == '\0')return srcPos;
        srcPos++;
        m++;
    }
    return 0;
}
*/
