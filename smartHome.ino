
/*
     PIN description
     D2(2) - motion sensor
     D3(3) - RX
     D4(4) - TX
     D5(5) - button pin
     D6(6) - button pin
     D7(7) - DDS18B20 pin( ONE_WIRE )

     D9(9) - reset for SIM900

*/

/*
  sYSTEM work with 3 clients maximum. First client is admin - it have rights on change
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
   phone - International number type (starts with '+' and have 11 digits)
     client 2 and client3 can`t mandatory


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

// redifene max buffer for SoftwareSerial otherwice
//  can`t set request to server(small buffer size
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

uint8_t coolThemperature = MIN_FIRING_THEMPERATURE;



void setup() {
  wdt_reset();
  wdt_disable();
  flags = 0b11100000;
  // start up serial port`s
  Serial.begin(9600);
#if DEBUG
  Serial.println(F("\n\tStart_Smart_Home_Apps v_0.04\n\tDEBUG\n "));
#endif
  // prepare device for write admin phone tophonebook
  // SIM
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, SIM_NO_RST);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // alarm
  //motion sensor
  pinMode(MOTION_SENSOR_PIN, INPUT);
  digitalWrite(MOTION_SENSOR_PIN, LOW);
  //  check outer voltage
  pinMode(VOLTAGE_PIN, INPUT);

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
  delay(200);
  modem.initModem();  // init sim module
  motionCounter = 0;
  coolThemperature = util.getCoolThemperature();
#if DEBUG
  Serial.println("COOL themperature = " + String(coolThemperature));
#endif
  handleSms();
  readThemperature();
}

void loop() {
  if (digitalRead(DUCT_PIN) == LOW && digitalRead(BUTTON_PIN) == LOW)buttonHandle();

  // ------- DUCT for swap beetwen serial and modem -----------
  if (digitalRead(DUCT_PIN) == LOW) {
    while (pipe.available() > 0)Serial.write(pipe.read());
    while (Serial.available() > 0)pipe.write(Serial.read());
  } else {

    // --------------- warnings SMS ---------------------
    // voltage
    if (digitalRead(VOLTAGE_PIN) == LOW
        && bitRead(flags, VOLTAGE_SEND_FLAG) == 1) {
      if (sendWarning(VOLTAGE))bitClear(flags, VOLTAGE_SEND_FLAG);
    }
    // motion
    if (bitRead(warningFlags, MOTION_SMS) == 1
        && bitRead(flags, MOTION_SEND_FLAG) == 1) {
      if (sendWarning(MOTION)) bitClear(flags, MOTION_SEND_FLAG);
    }
    //  cool firing
    if (bitRead(warningFlags, COOL_SMS) == 1
        && bitRead(flags, COOL_SEND_FLAG) == 1) {
      if (sendWarning(COOL))bitClear(flags, COOL_SEND_FLAG);
    }

    // ---------------  check themperature  -----------------
    if (bitRead(flags, CHECK_FLAG) == 1)readThemperature();
    delay(5);

    // ------------  LISTEN MODEM  ----------------------
    if (modem.available() > 0) handleModemMsg();

  }
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
   on click Button change reflect modem command
*/
void buttonHandle() {
  delay(20);
  int pressCounter = 0;
  for (uint8_t i = 0; i < 6; i++) {
    if (digitalRead(BUTTON_PIN) == LOW)pressCounter++;
    else pressCounter--;
    delay(20);
  }
  if (pressCounter > 2)modem.changeShowCommand();
  while (digitalRead(BUTTON_PIN) == LOW) {}
}


/*
   Set delay time for any GSM task
*/
void setEspiredTime(uint8_t delayTime) {
  noInterrupts();
  delayEspired = delayTime;
  interrupts();
}

/*
   Read themperature and set it to VoiceHandler
   if themperature small than MIN_FIRING_THEMPERATURE
    call func for send warning sms
*/
void readThemperature() {
  if (digitalRead(DUCT_PIN) == HIGH)digitalWrite(LED_PIN, LOW);
#if DEBUG
  Serial.print(F("\n\t readThemperature"));
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
  if (themperature < 1)themperature = 1;
  // set new data
  voice.setSensorsData(themperature, motionCounter);
#if DEBUG
  Serial.println("\n\t t= " + String(themperature) + "; motion= " + String(motionCounter) + " ;");
#endif
  if (themperature < coolThemperature && bitRead(flags, COOL_SEND_FLAG) == 1) {
    Serial.println(F("\tStart send cool SMS"));
    bitSet(warningFlags, COOL_SMS);
  }
  if (themperature > coolThemperature && bitRead(warningFlags, COOL_SMS) == 1)bitClear(warningFlags, COOL_SMS);
  handleSms();
}

/*
    send warnings sms
*/
bool sendWarning( int warningType ) {
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
  return res;
}


/*
   handle emited of  the modem out
*/
void handleModemMsg() {
  uint8_t code, ret;
  char phone[13];
  ret = util.getTypeRequest();
  switch (ret) {
    case INC_SMS:
      code = handleSms();
      break;
    case INC_CALL:
      code = voice.handleIncoming(phone);
      if (code == ASSIGN_ADMIN)break;
      handleSms(); break;
    case UNDEFINED: code = IS_EMPTY; modem.dropGSM(); break;
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
  modem.sendCommand(F("AT+GSMBUSY=1 \r"));
  modem.dropGSM();
  uint8_t code = util.handleIncomingSms();
  if (code == SEND_INFO_SMS) {
    sms.sendInfoSms();
  }
  if (code == SET_COOL_THEMPERATURE_RECORD)coolThemperature = util.getCoolThemperature();
  modem.sendCommand(F("AT+GSMBUSY=0 \r"));
  modem.dropGSM();
  return code;
}



/* ==============================================
                    INTERRUPTS
   ==============================================
*/

/*
  ISR handler for interrupt maked from motion sensor signals
*/
void motion_sensor_INTR_handler(void) {
  dropMotionTimer = DROP_MOTION_TIME;
  if (bitRead(flags, MOTION_SEND_FLAG) == 1 )bitSet(warningFlags, MOTION_SMS);
  if (motionCounter < 6) motionCounter++;
  else motionCounter = 1 ;
}


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
    // Serial.println("\n\nInside timer intr delayEspired = " + String(delayEspired) + "\n\n\n");
  }

  //

  //  check sensors values
  checkSensors++;
  if (checkSensors == CHECK_SENSOR_TICK  && digitalRead(VOLTAGE_PIN) == HIGH) {
    bitSet(flags, CHECK_FLAG);
    checkSensors = 0;
  }

  //  motion espired
  //  run motion sensor set espired time > 0
  // if espired time is 1 drop motion counter
  if (dropMotionTimer > 0) dropMotionTimer--;
  else motionCounter = 0;

  //     SEND SMS
  //  work with delay time beetwen send cool sms
  if (bitRead(flags, COOL_SEND_FLAG) == 0)coolEspired ++;
  if (coolEspired == DELAY_FOR_COOL) {
    bitSet(flags, COOL_SEND_FLAG);
    coolEspired = 0;
  }
  //  work with delay time beetwen send motion sms
  if (bitRead(flags, MOTION_SEND_FLAG) == 0)motionEspired ++;
  if (motionEspired == DELAY_FOR_MOTION) {
    bitSet(flags, MOTION_SEND_FLAG);
    motionEspired = 0;
  }
  //  work with delay time beetwen send drop voltage sms
  if (bitRead(flags, VOLTAGE_SEND_FLAG) == 0)voltageEspired ++;
  if (voltageEspired == DELAY_FOR_VOLTAGE) {
    bitSet(flags, VOLTAGE_SEND_FLAG);
    voltageEspired = 0;
  }

  // blink on DUCT
  if (digitalRead(DUCT_PIN) == LOW) digitalWrite( LED_PIN, !digitalRead(LED_PIN));

  //  WDT soft reset after 4 timerCounter loop
  timerCounter ++;
  if (timerCounter == 43200) {
    loopCounter++;
    timerCounter = 0;
    if (loopCounter == 4) {
      //  REBOOT the device after 2 days work
      wdt_enable (WDTO_8S);
      digitalWrite(RST_PIN, LOW);
      Serial.println(F("\n___________________________\n\tAFTER 8 seconds to be RESET\n___________________________"));
      delay(9000);

    }
  }
#if DEBUG
  //  WARNING - this action is only debug and may be leave to brokem programm work
  // it work wery randomly
  // if(timerCounter == 20) handleSms();
#endif
}
