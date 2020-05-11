#ifndef Constants_h
#define Constants_h

/*
 * ============================DEFINIONS==================================
 */

/*  debug mode ----- DEBUG ----- debug mode */
#define DEBUG 0

// critical values
#define MIN_FIRING_THEMPERATURE  30
#define UCS2_PHONE_SIZE 48

//  modem pins
#define RX_PIN   3
#define TX_PIN  4

// Pin for One wire
#define ONE_WIRE_BUS    7
// ----- motion sensors ----
#define MOTION_SENSOR_PIN  2
#define DROP_MOTION_TIME 900
// ---- signals led  ----
#define LED_PIN   13
// -----  voltage pin
#define VOLTAGE_PIN  14
// ---- tune button  ----
#define BUTTON_PIN    5
#define DUCT_PIN   6
#define MIN_PRESSED_COUNT 6
// reset
#define RST_PIN    9
// hard reset - SIM900 POWER OFF
#define SIM_RST     LOW  //  sim900 power reset level 0 - reset
#define SIM_NO_RST  HIGH //  level 1 - in work

// ----  check sensors time  ----
#if DEBUG==0
#define CHECK_SENSOR_TICK 180
#else
#define CHECK_SENSOR_TICK 60
#endif

// audio files
#define FIRING_SOUND  "firing.amr"
#define HOUSE_SOUND  "house.amr"
#define MOTION_SOUND "motion.amr"
#define BAD_SOUND   "bad.amr"
#define GOOD_SOUND "good.amr"
#define VAWE_SOUND "vawe.amr"
#define CAT_SOUND "cat.amr"
#define THERMO_BAD "termoBad.amr"
#define NO_VOLTAGE "noVoltage.amr"

//   delays beetwen send sms
#define DELAY_FOR_VOLTAGE  32400  // 9 hour beetwen send outer voltage is noll (0v)
#define DELAY_FOR_COOL  32400   // 9 hour beetwen send very small firing themperature
#define DELAY_FOR_MOTION 14400  // 4 hour beetwen send motion in house
// delay times (wait for module answer)
#define DELAY_FOR_OK  2
#define DELAY_FOR_ANSWER  5
#define DELAY_FOR_SOUND  10

//  SMS formats
#define GSM_FORMAT "GSM"
#define UCS2_FORMAT "UCS2"

// send SMS
#define SEND_SMS 1
#define NO_SEND_SMS 0
/* =======================CONSTANTS========================== */

// return codes
enum retCodes {
  SUCCESS = 0,
  NO_SEND = 1,
  IS_EMPTY = 2,
  WRONG_DATA = 3,
  WRONG_TASK = 4,
  NO_PLAY_SOUND = 5,
  EXIT = 6,
  ASSIGN_ADMIN = 7,
  RETURN_ERROR = 8,
  WRONG_USER = 9,
  SEND_INFO_SMS = 10,
  SET_COOL_THEMPERATURE_RECORD = 11
};

// clients ( first record is admin )
enum clients {
  ADMIN = 1,
  CLIENT1 = 2,
  CLIENT2 = 3,
  COOL_THEMPERATURE_RECORD = 4,
  IS_SEND_MOTION_RECORD = 5,
};

// types modem requests
enum requests {
  INC_SMS = 1,
  INC_CALL = 2,
  UNDEFINED = 3
};


/*   DATA in flash  */
//  SMS
#define MAX_MESS_LEN 90
//  message on cool themperature "Отопление не работает"; 85 symbols
const char  cool[] PROGMEM =  "041e0442043e043f043b0435043d043804350020043d04350020044004300431043e04420430043504420021\0";
//  message on trigged motion sensor "Движение в доме" ; 61 symbols
const char  motion[] PROGMEM = "04140432043804360435043d043804350020043200200434043e043c0435\0";
//  Turn off voltage  "Пропало напряжение!"  77 symbols
const char  voltage[] PROGMEM = "041f0440043e043f0430043b043e0020043d0430043f0440044f04360435043d043804350021\0";

const char *const messages[] PROGMEM = { cool, motion, voltage };
//  for choose messages
enum { COOL, MOTION, VOLTAGE };

//  PUBLIC FUNCTIONS
//  need for wait of the answer from modem
void setEspiredTime(uint8_t delayTime);

#endif
