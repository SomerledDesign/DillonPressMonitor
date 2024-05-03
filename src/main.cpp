#include <Arduino.h>
#include <LiquidCrystal.h>
#include <OneButton.h> // to implement 'LONG PRESS' on the control buttons
#define DEBUG 1

/* TODO TREE */
// [ ]: this is a draft issue
// [x]: this is a solved issue
// BUG: This is a bug
// SPLAT: This is a bug
// ROACH: This is a bug
// FIXME: This needs to be fixed....
// FIXIT: This needs to be fixed....
// FIX: This needs to be fixed....
// TODO: This needs done
// HACK: This is a hack
// IN PROGRESS: This needs to be updated

#if DEBUG == 1
  #define BAUD  9600
  #define serialbegin(x)  Serial.begin(x)
  #define debug(x)	Serial.print(x)
  #define debugln(x)	Serial.println(x)
#else
  #define BAUD
  #define debug(x)
  #define debugln(x)
  #define serialbegin(BAUD)
#endif // DEBUG

/*
    Statistics Monitored
      Loaded Rounds           --> Rnds
      Press Time              --> Time
      Rounds Per Hour Current --> RPHc      (last 3-15 rounds)
      Rounds Per Hour Total   --> RPHt
      Remaining Rounds        --> RmRd
      Remaining Press Time    --> RmTm
      Powder Measure Grains   --> PmGr
      Powder Measure Rounds   --> PmRd


    power on
    draw statistical data from eeprom

    is this the first power up?

    display flash screen with data
    are we in a session already?
    display session data
    hold ignore button down 3 sec to start a new session
    do session set up
    wait for sensor input
    react to sensor data
    increment variables (Rnds)
    calculate variables (RNDc, RNDh, Time, )

*/
//  ERRORS:
//  A collection of errors which could occur during reloading and their solutions

// BAD SEQUENCE: Displayed when any unexpected action occurs. The Press Monitor
// expects a very specific sequence of actions and if the actions deviate at all,
// this error is generated.

// ROTATE BEFORE PRIME
// DOUBLE ROTATE: Displayed when the shellplate is rotated twice by mistake on
// a manual indexing press. This is a very dangerous condition as it usually
// leads to a no charge event.

// NO ROTATE: Displayed when the press handle is cycled twice without rotating
// the shellplate on a manual indexing press. This is a very dangerous condition
// as it usually leads to a double charge event.

// NO PRIME: Displayed the prime action was missed.

// SHORT STROKE: Displayed when the press handle is not pulled fully down. Can
// cause a round to not have any powder or in it, or less powder that desired
// if the powder measure was not fully activated.

// ERROR 1, 2, or 3: Displayed when any sensor is configured as an error sensor
// and that sensor is activated.

/*  Digital ouput pins
 *
 *
 *           /$$$$$$$  /$$           /$$   /$$               /$$
 *          | $$__  $$|__/          |__/  | $$              | $$
 *          | $$  \ $$ /$$  /$$$$$$  /$$ /$$$$$$    /$$$$$$ | $$
 *          | $$  | $$| $$ /$$__  $$| $$|_  $$_/   |____  $$| $$
 *          | $$  | $$| $$| $$  \ $$| $$  | $$      /$$$$$$$| $$
 *          | $$  | $$| $$| $$  | $$| $$  | $$ /$$ /$$__  $$| $$
 *          | $$$$$$$/| $$|  $$$$$$$| $$  |  $$$$/|  $$$$$$$| $$
 *          |_______/ |__/ \____  $$|__/   \___/   \_______/|__/
 *                         /$$  \ $$
 *                        |  $$$$$$/
 *                        \_______/
 *
 */
// Number indicates IO## from ESP32 MiniKit Github
// https://github.com/MHEtLive/ESP32-MINI-KIT/blob/master/Shield%20libraries/ESP32/examples/Timer/RepeatTimer/RepeatTimer.ino

const uint8_t lcd_rs = 25; // LCD Reset
const uint8_t lcd_rw = 27; // LCD R/W
const uint8_t lcd_en = 32; // LCD Enable
const uint8_t lcd_d0 = 22; // Data Pin 4
const uint8_t lcd_d1 = 21; // Data Pin 5
const uint8_t lcd_d2 = 12; // Data Pin 6
const uint8_t lcd_d3 = 4;  // Data Pin 7
const uint8_t blctrl_pin = 17;   // Backlight PWM pin
const uint8_t mode_led_pin = 18;
const uint8_t warn_buzzer_pin = 19;
const uint8_t warn_led_bot_pin = 26;
const uint8_t warn_led_top_pin = 36;
// TODO: Check if is correct...
// if this button pressed when device off it turns on mosfet
// soft power circuit.
// if held for 3 secs when power is on, when released power_ctrl_pin
// [OneButton function PowerOff()]
// goes low turning off soft power.
const uint8_t power_ctrl_pin  = 39; // Power Control IO 39

#define TRUE 1
#define FALSE 0

const uint8_t lcd_columns = 16;
const uint8_t lcd_rows = 2;


//    *************************************************************************
//    ***                        Digital input pins                         ***
//    *************************************************************************
// ============================================================================
//    BUTTONS    BUTTONS    BUTTONS    BUTTONS    BUTTONS    BUTTONS    BUTTONS
// ============================================================================
const uint8_t button1_pin = 17; // POWER
const uint8_t button2_pin = 0;  // MINUS
const uint8_t button3_pin = 16; // IGNORE
const uint8_t button4_pin = 2;  // PLUS

// ============================================================================
//    SENSORS    SENSORS    SENSORS    SENSORS    SENSORS    SENSORS    SENSORS
// ============================================================================
const uint8_t sensor1_pin = 33; // UP Sensor
const uint8_t sensor2_pin = 35; // DOWN Sensor
const uint8_t sensor3_pin = 14; // PRIME Sensor

/* HACK:
   *** This pin is number 9 on the pc board,
   *** it is number 13 in WOKWI.  
   *** This will be changed in the production
   *** software to match the wired PC Board.
*/
const uint8_t sensor4_pin = 13;  // ROTATE Sensor  

/* Global variables
 *
 *                ______   __            __                  __
 *               /      \ /  |          /  |                /  |
 *              /$$$$$$  |$$ |  ______  $$ |____    ______  $$ |  _______
 *              $$ | _$$/ $$ | /      \ $$      \  /      \ $$ | /       |
 *              $$ |/    |$$ |/$$$$$$  |$$$$$$$  | $$$$$$  |$$ |/$$$$$$$/
 *              $$ |$$$$ |$$ |$$ |  $$ |$$ |  $$ | /    $$ |$$ |$$      \
 *              $$ \__$$ |$$ |$$ \__$$ |$$ |__$$ |/$$$$$$$ |$$ | $$$$$$  |
 *              $$    $$/ $$ |$$    $$/ $$    $$/ $$    $$ |$$ |/     $$/
 *               $$$$$$/  $$/  $$$$$$/  $$$$$$$/   $$$$$$$/ $$/ $$$$$$$/
 */
const uint8_t DELAY_MIN = 10;
const uint16_t DELAY_MAX = 10000;
const uint16_t DELAY_BUTTON_HOLD = 3000;

LiquidCrystal lcd(lcd_rs, lcd_rw, lcd_en, lcd_d0, lcd_d1, lcd_d2, lcd_d3);

OneButton Power_Button(button1_pin);
OneButton Minus_Button(button2_pin);
OneButton Ignore_Button(button3_pin);
OneButton Plus_Button(button4_pin);

uint8_t sensor1Status = 0; // push-up
uint8_t sensor2Status = 0; // pull-down
uint8_t sensor3Status = 0; // prime
uint8_t sensor4Status = 0; // rotate
uint8_t machineState = 0;
uint8_t lastSensorActivated = 0;

enum HandleState {
    UNKNOWN,
    AT_REST,
    DOWN,
    PRIME
};

HandleState currentHandleState = AT_REST;

// Function Declarations
//      ____            __                 __  _
//     / __ \___  _____/ /___ __________ _/ /_(_)___  ____  _____
//    / / / / _ \/ ___/ / __ `/ ___/ __ `/ __/ / __ \/ __ \/ ___/
//   / /_/ /  __/ /__/ / /_/ / /  / /_/ / /_/ / /_/ / / / (__  )
//  /_____/\___/\___/_/\__,_/_/   \__,_/\__/_/\____/_/ /_/____/
//

// Power Button function(s)
void ledflash();    // when just pushing the power button for less than 3 seconds
void PowerDOWN();  // Held for 3 Seconds it should drive power_ctrl_pin LOW to turn off Mosfet

// Minus Button functions
void DecrementValue();
void TestSensors(); // Held for 3 Seconds

// Ignore Button functions
void IgnoreSensorInput();
void MonitorSetup(); // Held for 3 Seconds

// Plus Button functions
void IncrementValue();
void SessionSetup(); // Held for 3 Seconds

// Main functions
uint8_t CheckSensors();
void Error(uint8_t errnum);

// Utility functions
void FlashLEDs(uint8_t flashTimes, uint16_t flashDelay, bool beep);

String ErrorMsg[9] = {"BAD SEQUENCE", "DOUBLE ROTATE", " NO ROTATE", " NO PRIME", "SHORT STROKE", " ERROR 1", " ERROR 2", " ERROR 3"};
// String Sequence[6] = {"UNKNOWN CONDTION", "Pull Handle Down", "Push UP", "Prime", "Rotate Index"};

/*  Useful Functions
 *
 *        ________                                 __      __
 *       |        \                               |  \    |  \
 *       | $$$$$$$$__    __  _______    _______  _| $$_    \$$  ______   _______    _______
 *       | $$__   |  \  |  \|       \  /       \|   $$ \  |  \ /      \ |       \  /       \
 *       | $$  \  | $$  | $$| $$$$$$$\|  $$$$$$$ \$$$$$$  | $$|  $$$$$$\| $$$$$$$\|  $$$$$$$
 *       | $$$$$  | $$  | $$| $$  | $$| $$        | $$ __ | $$| $$  | $$| $$  | $$ \$$    \
 *       | $$     | $$__/ $$| $$  | $$| $$_____   | $$|  \| $$| $$__/ $$| $$  | $$ _\$$$$$$\
 *       | $$      \$$    $$| $$  | $$ \$$     \   \$$  $$| $$ \$$    $$| $$  | $$|       $$
 *        \$$       \$$$$$$  \$$   \$$  \$$$$$$$    \$$$$  \$$  \$$$$$$  \$$   \$$ \$$$$$$$
 *
 *
 */

//    *************************************************************************
//    ***                       Button Press Actions                        ***
//    *************************************************************************
//
//  ------------
//  Power Button
//  ------------

void ledflash()
{
  FlashLEDs(3,200,FALSE);
}

void PowerDOWN()
{
  debug(".");
  delay(50);
  debug(".");
  delay(50);
  debug(".");
  delay(50);
  debugln("Powering DOWN");
  delay(500);
  pinMode(power_ctrl_pin, OUTPUT);
  digitalWrite(power_ctrl_pin, LOW);
}

//  ------------
//  Minus Button
//  ------------
void DecrementValue()
{
  debugln("...Decrementing Value");
}
void TestSensors()
{
  debugln("Testing Sensors");
}

//  ------------- 
//  Ignore Button
//  -------------
void IgnoreSensorInput()
{
  debugln("Ignoring Sensors");
}
void MonitorSetup()
{
  debugln("Entering Monitor Setup...");
}

//  -----------
//  Plus Button
//  -----------
void IncrementValue()
{
  debugln("Incrementing Value...");
}

void SessionSetup()
{
  debugln("...Entering Session Setup");
}

// IN PROGRESS: This needs to be figured out
//  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
//  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
//  * *       \'/    _____                                                * *
//  * *     -- + -- / ___/___  ____  _________  _____                     * *
//  * *       /.\   \__ \/ _ \/ __ \/ ___/ __ \/ ___/      \     /        * *
//  * *            ___/ /  __/ / / (__  ) /_/ / /           *   *         * *
//  * *           /____/\___/_/ /_/____/\____/_/             \ /          * *
//  * *                     / /   ____  ____ _(_)____  --*--- + ---*--    * *
//  * *                    / /   / __ \/ __ `/ / ___/        / \          * *
//  * *                   / /___/ /_/ / /_/ / / /__         *   *         * *
//  * *                  /_____/\____/\__, /_/\___/        /     \        * *
//  * *                              /____/                               * *
//  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
//  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
//
//                       UP         DOWN      PRIME       ROTATE    machineState                  TRUE if->
//                    Sensor 1    Sensor 2   Sensor 3    Sensor 4
//  Handle at Rest       x           o          o           o            0/1/4      ==> if  lastSensorActivated == 4
//  Handle Pull Down     o           x          o           o            1          ==> if  lastSensorActivated == 1
//  Handle to Prime      x           o          x           o            2          ==> if  lastSensorActivated == 2
//  Rotate Index         x           o          o           x            3          ==> if  lastSensorActivated == 3
//
//    The operation is backwards in that the down stroke raises the shaft/shellplate.
//    We will use the position of the handle to refer to different operations, not the
//    location/movement of the shaft.
//    On Start-->
//      UP sensor Makes
//    On the down stroke the shell is de-primed and resized.
//      UP sensor Breaks
//      DOWN sensor Makes ...at bottom of travel
//    On the up stroke the spent primer drops into the cup.
//      DOWN sensor Breaks
//      UP sensor Makes ...at top of travel
//    Pushing the handle fully up/forward seats a new primer.
//      PRIME sensor Makes
//      UP sensor Still Makes
//    Release forward pressure on handle.
//      PRIME sensor Breaks
//      UP sensor Still Makes
//
//
//    If no sensors are made, machine is in 'UNKNOWN CONDITION'
//
//    What if I did one 'if' for each condition, as shown in truth table above?
//    Perhaps a While(sensor1 == low)...?
//
// Sensor Management
// ============================================================================
//     SENSOR    SENSOR    SENSOR    SENSOR    SENSOR    SENSOR    SENSOR
// ============================================================================

uint8_t CheckSensors()
{
//   sensor1Status = digitalRead(sensor1_pin); // UP Sensor
//   sensor2Status = digitalRead(sensor2_pin); // DOWN Sensor
//   sensor3Status = digitalRead(sensor3_pin); // PRIME Sensor
//   sensor4Status = digitalRead(sensor4_pin); // ROTATE Sensor
//   if (sensor1Status == LOW)
//   {
//     /* Condition One - Handle UP Sensor Made */
//     if (sensor3Status == HIGH && (machineState == MACHINE_AT_REST || machineState == MACHINE_INDEXED))
//     {
//       if (lastSensorActivated == 4)
//       {/* Start position */
//         lcd.setCursor(0, 0);
//         lcd.print("Pull Handle Down");
//         if (sensor2Status == LOW)
//         {
//           machineState = MACHINE_DEPRIMED;
//         }
//       }
//       else{
//           machineState = MACHINE_AT_REST;
//       }
//       delay(DELAY_MIN);
//     }
//     /* Condition Two - Handle Forward Prime Sensor Made */
//     else if (machineState == MACHINE_DEPRIMED && sensor3Status == LOW) // Handle in Prime
//     {
//       /* De-Primed Postion*/
//       lcd.setCursor(0, 0);
//       lcd.print("Prime"); //
//       machineState = MACHINE_PRIMED;
//       delay(DELAY_MIN);
//     }
//     /* Condition Three - Shellplate Indexed - Rotate Sensor Made */
//     else if (machineState == MACHINE_PRIMED && sensor4Status == HIGH) // ROTATE
//     {
//       /* De-Primed Postion*/
//       lcd.setCursor(0, 0);
//       lcd.print("Index Shellplate");
//       if (sensor4Status == LOW){
//         machineState = MACHINE_AT_REST;
//       }
//       delay(DELAY_MIN);
//     }
//     else
//     {
//       /* code */
//     }
//   }
//   else if (sensor3Status == LOW && machineState == MACHINE_AT_REST)
//   { // Shell deprimed, Handle down
//     lcd.setCursor(0, 0);
//     lcd.print("Push Handle Up"); // Push UP
//     // lcd.print(Sequence[MACHINE_DEPRIMED]); // Push UP
//     machineState = MACHINE_DEPRIMED;
//     delay(DELAY_MIN);
//   }
//   else if (sensor1Status == LOW && machineState == MACHINE_DEPRIMED)
//   {
//     lcd.setCursor(0, 0);
//     lcd.print("Prime"); // Prime
//     // lcd.print(Sequence[MACHINE_PRIMED]); // Prime
//     machineState = MACHINE_PRIMED;
//     delay(DELAY_MIN);
//   }
//   else if ((sensor1Status == LOW && sensor3Status == LOW) && machineState == MACHINE_PRIMED)
//   {
//     lcd.setCursor(0, 0);
//     lcd.print("Rotate Index"); // Rotate
//     // lcd.print(Sequence[3]); // Rotate
//     machineState = MACHINE_INDEXED;
//     delay(DELAY_MIN);
//   }
//   else if (sensor1Status == LOW && machineState == MACHINE_INDEXED)
//   {
//     lcd.setCursor(0, 0);
//     lcd.print("Rnd Cnt +1"); // Prime
//     // lcd.print(Sequence[3]); // Prime
//     machineState = MACHINE_AT_REST;
//   }
//   // else if (sensor1Status == HIGH && sensor2Status == HIGH && sensor3Status == HIGH && sensor4Status == HIGH)
//   // {
//   //   // no sensors active
//   //   lcd.print("UNKNOWN CONDITION");
//   // }
//   else if (sensor1Status == LOW && machineState != 0)
//   {
//     Error(1);
//   }
//   else if (sensor2Status == LOW && (machineState == 0 || machineState == 2))
//   {
//     Error(2);
//   }
//   else if (sensor3Status == LOW && machineState != 2)
//   {
//     Error(3);
//   }
  return 0;
}


void FlashLEDs(uint8_t flashTimes, uint16_t flashDelay, bool beep)
{
  // Flash the crossing guard warning indicators for xx period of time
  uint8_t ctr1 = 0;
  for (ctr1 = 0; ctr1 <= flashTimes; ctr1 += 1)
  {
    // Top LED on, Bottom LED off
    digitalWrite(warn_led_top_pin, HIGH);
    digitalWrite(warn_led_bot_pin, LOW);

    // if beep is true buzz the buzzer only on this cycle
    if (beep == true)
    {
      tone(warn_buzzer_pin, 415, 250); // Plays 415Hz tone for 0.250 seconds;
    }
    delay(flashDelay);
    
    // Inner lights on, outer lights off
    digitalWrite(warn_led_top_pin, LOW);
    digitalWrite(warn_led_bot_pin, HIGH);
    delay(flashDelay);
  }
  // all lights off at end of sequence
  digitalWrite(warn_led_top_pin, LOW);
  digitalWrite(warn_led_bot_pin, LOW);
}

// ============================================================================
//     SETUP    SETUP    SETUP    SETUP    SETUP    SETUP    SETUP    SETUP
// ============================================================================
void setup()
{
  //TODO: Soft latch setup
  // set power control high to latch mosfet
  // pinMode(power_ctrl_pin, INPUT_PULLUP);
  // pinMode(blctrl_pin, OUTPUT);

  // put your setup code here, to run once:

  serialbegin(BAUD);


  // Initialize the Buttons
  Power_Button.setPressTicks(DELAY_BUTTON_HOLD);  // Set long press to be 3 seconds
  Power_Button.attachClick(ledflash);             //
  Power_Button.attachLongPressStop(PowerDOWN);    // Drive power_ctrl_pin low and go to sleep

  Minus_Button.setPressTicks(DELAY_BUTTON_HOLD);  // Set long press to be 3 seconds
  Minus_Button.attachClick(DecrementValue);       // Wake from sleep
  Minus_Button.attachLongPressStart(TestSensors); //

  Ignore_Button.setPressTicks(DELAY_BUTTON_HOLD); // Set long press to be 3 seconds
  Ignore_Button.attachClick(IgnoreSensorInput);   // Wake from sleep
  Ignore_Button.attachLongPressStop(MonitorSetup);// Enter setup

  Plus_Button.setPressTicks(DELAY_BUTTON_HOLD);   // Set long press to be 3 seconds
  Plus_Button.attachClick(IncrementValue);        // Wake from sleep
  Plus_Button.attachLongPressStop(SessionSetup);  //

  // sensor pin setup
  pinMode(sensor1_pin, INPUT_PULLUP);
  pinMode(sensor2_pin, INPUT_PULLUP);
  pinMode(sensor3_pin, INPUT_PULLUP);
  pinMode(sensor4_pin, INPUT_PULLUP);

  // output pin setup
  pinMode(warn_led_top_pin, OUTPUT);
  pinMode(warn_led_bot_pin, OUTPUT);
  pinMode(mode_led_pin, OUTPUT);
  pinMode(warn_buzzer_pin, OUTPUT);

  // Initialize the LCD
  lcd.begin(lcd_columns, lcd_rows);
  lcd.setCursor(2, 0);
}

// ============================================================================
//      LOOP    LOOP    LOOP    LOOP    LOOP    LOOP    LOOP    LOOP    LOOP
// ============================================================================
void loop()
{
  // put your main code here, to run repeatedly:

  Power_Button.tick();
  Minus_Button.tick();
  Ignore_Button.tick();
  Plus_Button.tick();
  CheckSensors();
}

// ============================================================================
//      ERRORS    ERRORS    ERRORS    ERRORS    ERRORS    ERRORS    ERRORS
// ============================================================================
void Error(uint8_t errnum)
{
  lcd.setCursor(0, 0);

  // the standard offset
  // for the longer text
  // One 13 two 12's
  uint8_t offset = 2;
  uint8_t offset2 = 14;

  // draw the box around the screen
  lcd.write((byte)0xFF);
  lcd.write((byte)0xFF);

  // what is the length of this errorMsg string?
  if (ErrorMsg[errnum].length() < 9)
  {
    lcd.write((byte)0xFF);
    lcd.setCursor(offset2, 0);
    lcd.write((byte)0xFF);
    lcd.write((byte)0xFF);

    offset = 4;
    offset2 = 13;
  }
  else if (ErrorMsg[errnum].length() < 11)
  {
    lcd.write((byte)0xFF);
    offset = 3;
    /* code */
  }

  lcd.setCursor(offset2, 0); // set cursor to first line 2nd charactor
  lcd.write((byte)0xFF);
  lcd.write((byte)0xFF);

  lcd.setCursor(0, 1); // set cursor to first line 2nd charactor
  lcd.write((byte)0xFF);

  lcd.setCursor(offset, 0); // set cursor to first line 2nd charactor
  lcd.print(ErrorMsg[errnum]);

  lcd.setCursor(1, 1);
  lcd.print("CHECK STATIONS");
  lcd.setCursor(15, 1); // set cursor to second line end charactor
  lcd.write((byte)0xFF);

  FlashLEDs(5, 400, TRUE);
}
