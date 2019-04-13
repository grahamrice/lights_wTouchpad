#include <EEPROM.h>

int led = 13;

/* Define the digital pins used for the clock and data */
#define SCL_PIN 8
#define SDO_PIN 9

#define DOOR_SW_PIN 2

#define RGB_RED 3
#define RGB_GREEN 5
#define RGB_BLUE 6

#define TOUCH_RED   10
#define TOUCH_GREEN 11
#define TOUCH_BLUE  12

#define EEPROM_START_ADDR 0xC0

/* Used to store the key state */

int writeToEepromFlag = 0;
int restoreFromEepromFlag = 0;

short display_flags = 0;
#define DOOR_SW 1
#define CTRL_ON 2

byte switchFadeTimer = 100;

int savedRed = 0;
int savedGreen = 0;
int savedBlue = 0;
int liveRed, liveGreen, liveBlue;

//-----------------commands--------------------
#define NO_OP      0
#define INC_RED    1
#define INC_GREEN  2
#define INC_BLUE   4
#define OP_SAVE    15
#define OP_RESTORE 5
#define OP_TOGGLE  7
#define OP_ABORT   255

/*-------------------------Set Leds----------------------------------*/

void setRgbLeds()
{
  analogWrite(RGB_RED, liveRed & 0xff);
  analogWrite(RGB_GREEN, liveGreen & 0xff);
  analogWrite(RGB_BLUE, liveBlue & 0xff);
}

void setRgbLedsOff()
{
  setRgbLeds(0,0,0);
}

void setRgbLedsSaved()
{
  liveRed = savedRed;
  liveGreen = savedGreen;
  liveBlue = savedBlue;
 // setRgbLeds(savedRed, savedGreen, savedBlue);
}

/*---------------------Read EEPROM------------------------*/
int getColour(int address, byte * value)
{
  byte val1, val2;
  val1 = EEPROM[address];
  val2 = EEPROM[address+1];
  if((0xff - val1) == val2)
  {
    *value = val1;
    return 0;
  }else{
    *value = 0;
    return -1;
  }
}

void getRgbEeprom()
{
  byte r,g,b;
  int success = 0;
  success += getColour(EEPROM_START_ADDR + 0, &r);
  success += getColour(EEPROM_START_ADDR + 2, &g);
  success += getColour(EEPROM_START_ADDR + 4, &b);

  if(success == 0)
  {
    savedRed   = r;
    savedGreen = g;
    savedBlue  = b;
    if(doorSwitch) setRgbLedsSaved();
    Serial.println("Colours found in EEPROM");    
  }else{
    savedRed   = 0;
    savedGreen = 0;
    savedBlue  = 0;     
    Serial.println("Data in EEPROM invalid");
  }
}

/*------------------------Write EEPROM-------------------------*/

void setRgbEeprom(){
  byte colours[6];
  colours[0] = (byte)savedRed;
  colours[1] = (byte)(0xff - savedRed);
  colours[2] = (byte)savedGreen;
  colours[3] = (byte)(0xff - savedGreen);
  colours[4] = (byte)savedBlue;
  colours[5] = (byte)(0xff - savedBlue);

  for(int y = 0; y < 6; y++)
  {
    EEPROM[y+EEPROM_START_ADDR] = colours[y];
  }

  Serial.println("Saved to EEPROM");
}

/*------------------------Flag checking------------------------*/
void checkEepromFlags()
{
   if(writeToEepromFlag){
    Serial.println("Long press on 1 detected"); /* now you just need to write a write to EEPROM function here */
    setRgbEeprom();
    writeToEepromFlag = 0;
   }

   if(restoreFromEepromFlag){
    Serial.println("Long press on 5 detected");
    restoreFromEepromFlag = 0;
    getRgbEeprom();
    setRgbLedsSaved();
   }
}

/*------------------Setup-------------------------------*/
void setup()
{
  /* Initialise the serial interface */
  Serial.begin(115200);
  /* Configure the clock and data pins */
  pinMode(led, OUTPUT);
  pinMode(SCL_PIN, OUTPUT);  
  pinMode(SDO_PIN, INPUT);
  pinMode(RGB_RED,OUTPUT); 
  pinMode(RGB_GREEN,OUTPUT); 
  pinMode(RGB_BLUE,OUTPUT); 

  pinMode(TOUCH_RED,  INPUT);
  pinMode(TOUCH_GREEN,INPUT);
  pinMode(TOUCH_BLUE, INPUT);
  
  getRgbEeprom();


}

/*-----------------------Dump EEPROM------------------------*/

void dumpEeprom()
{
  for(int e = 0; e < EEPROM.length(); e++){
    Serial.println(EEPROM[e],HEX);
  }
}

#define incrementValue 0x20

/* Main program */
void loop()
{
  /* Read the current state of the keypad */
  byte lastReadKey;
  byte Key = Read_Keypad();

  doorSwitch = (~digitalRead(DOOR_SW_PIN)) & 0x1; //invert, floats to 12V when light off, 0v when on
  //change to set flag in display_flags
//include fade timer and such somehow
  
  switch(Key){
    case INC_RED:   liveRed += incrementValue;
                    break;
    case INC_GREEN: liveGreen += incrementValue;
                    break;
    case INC_BLUE:  liveBlue += incrementValue;
                    break;
    case OP_SAVE: savedRed   = liveRed;
                  savedGreen = liveGreen;
                  saveBlue   = liveBlue;
                  setRgbEeprom();
                  break;
    case OP_RESTORE: setRgbLedsSaved();
                     break;
    case OP_TOGGLE: if((display_flags & CTRL_ON) == CTRL_ON) display_flags &= ~CTRL_ON;
                    else                                     display_flags |= CTRL_ON;
                    break;
    case NO_OP:
    default: break; //do nothing
  }

  if((display_flags & DOOR_SW) == DOOR_SW){
    setRgbLeds();
  }
  else{
    if((display_flags & CTRL_ON) == CTRL_ON){
      setRgbLeds();
      //do timer stuff to ensure it doesn't stay on forever
    }
  }
  
  delay(100);
}

#define PRESS_HOLD 5
#define LONG_PRESS 10

/* Read the state of the keypad */
byte Read_Keypad(void)
{
  static byte last_press = 0;
  static short press_count = PRESS_HOLD;
  byte this_press = 0;

  if(digitalRead(TOUCH_RED  )) this_press |= INC_RED;
  if(digitalRead(TOUCH_GREEN)) this_press |= INC_GREEN;
  if(digitalRead(TOUCH_BLUE )) this_press |= INC_BLUE;

  if(this_press == 0){
    press_count = PRESS_HOLD;
    if((last_press < 5)||(last_press == OP_ABORT)) { //allow abort or last command to clear
      last_press = 0;
      return 0;
    }
     if(press_count > 0){
     this_press = last_press; /*assuming this is OP_TOGGLE*/
     last_press = 0; /*return that value */
     return this_press;
    }
  }
  if(this_press < 5){
    if(last_press == 0){
      press_count = PRESS_HOLD; /*first touch of button, return the colour*/
      last_press = this_press;
      return this_press;
    }
    if(last_press != this_press){
      last_press = OP_ABORT; /*another button has been pressed too, wait for release*/
      return 0;
    }
    press_count--;
    if(press_count == 0){ /*or we've counted down, so increment colour by returning that value*/
      press_count = PRESS_HOLD;
      return this_press;
    }
  }
  if(this_press == OP_RESTORE){
    if(last_press == 0){
      last_press = this_press;
      press_count = PRESS_HOLD;
      return 0;
    }
    if(last_press != this_press){
      last_press = OP_ABORT; /*another button has been pressed too, wait for release*/
      return 0;
    }
    press_count--;
    if(press_count == 0){
      last_press = OP_ABORT; /*timer cleared so send restore command*/
      return OP_RESTORE;
    }else{
      return 0;
    }
  }
  if(this_press == OP_TOGGLE){
    if(last_press == 0){
      press_count = LONG_PRESS;
      return 0;
    }
    if(last_press != this_press){
      last_press = OP_ABORT; /*another button has been pressed too, wait for release*/
      return 0;
    }
    press_count--;
    if(press_count == 0){
      last_press = OP_ABORT; /*code will save and flash twice*/
      return OP_SAVE;
    }
  }
}
