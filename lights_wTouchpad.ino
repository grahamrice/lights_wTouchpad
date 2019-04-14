#include <EEPROM.h>

int led = 13;

#define DOOR_SW_PIN 2

#define RGB_RED 3
#define RGB_GREEN 5
#define RGB_BLUE 6

#define TOUCH_CTRL   8
#define TOUCH_RED   10
#define TOUCH_GREEN 11
#define TOUCH_BLUE  12

#define EEPROM_START_ADDR 0xC0

/* Used to store the key state */

int writeToEepromFlag = 0;
int restoreFromEepromFlag = 0;

#define DOOR_SW    1
#define CTRL_ON    2
#define FLASH_LEDS 4
#define FADE_ON    8
#define FADE_OFF   16
short display_flags = CTRL_ON;
short display_fade = 0;
short display_flash = 0;

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
#define OP_TOGGLE  8
#define OP_SAVE    15
#define OP_RESTORE 5
#define OP_ABORT   255

/*-------------------------Set Leds----------------------------------*/

void setRgbLeds(byte f)
{
  analogWrite(RGB_RED, liveRed & f);
  analogWrite(RGB_GREEN, liveGreen & f);
  analogWrite(RGB_BLUE, liveBlue & f);
}

void setRgbLedsOff()
{
  analogWrite(RGB_RED, 0);
  analogWrite(RGB_GREEN, 0);
  analogWrite(RGB_BLUE, 0);
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
    //if(doorSwitch) setRgbLedsSaved();
    Serial.println("Colours found in EEPROM");
    Serial.println(r,DEC);    
    Serial.println(g,DEC);    
    Serial.println(b,DEC);    
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

 /* for(int y = 0; y < 6; y++)
  {
    EEPROM[y+EEPROM_START_ADDR] = colours[y];
  }*/

  Serial.println("Saved to EEPROM");
}

/*------------------Setup-------------------------------*/
void setup()
{
  /* Initialise the serial interface */
  Serial.begin(115200);
  /* Configure the clock and data pins */
  pinMode(led, OUTPUT);
  pinMode(RGB_RED,OUTPUT); 
  pinMode(RGB_GREEN,OUTPUT); 
  pinMode(RGB_BLUE,OUTPUT); 

  pinMode(TOUCH_CTRL, INPUT);
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

/*---------------Control led display------------------------*/

void control_leds(){
  if((display_flags & FLASH_LEDS) == FLASH_LEDS){ //highest priority
    if(display_flash == 0) display_flash = 45;
    display_flash--;
    if(display_flash > 30) setRgbLedsOff();
    if(display_flash > 15) setRgbLeds(0xff);
    if(display_flash >  0) setRgbLedsOff();
    if(display_flash == 0) display_flags &= ~FLASH_LEDS;
     
  }
  if((display_flags & CTRL_ON) == CTRL_ON){
    setRgbLeds(0xff);
  }
  if((display_flags & FADE_ON) == FADE_ON){
    if(display_fade == 0xff) display_fade = 0x00; //chances are this will not happen
    display_fade += 4;
    if(display_fade > 0xff) {
      display_fade = 0xff;
      display_flags &= ~FADE_ON;
    }
    setRgbLeds(display_fade);
  }
  if((display_flags & FADE_OFF) == FADE_OFF){
    if(display_fade == 0) display_fade = 0xff;
    display_fade -= 4;
    if(display_fade < 0) {
      display_fade = 0;
      display_flags &= ~FADE_OFF;
    }
    setRgbLeds(display_fade);
  }
  if((display_flags & DOOR_SW) == DOOR_SW){
    setRgbLeds(0xff);
  }

  setRgbLedsOff();
  return;                                         //lowest priority
}

#define incrementValue 0x20

/*------------------ Main program --------------------------*/
void loop()
{
  /* Read the current state of the keypad */
  byte lastReadKey;
  byte doorSwitch;
  byte Key = Read_Keypad();
  static byte flash_strip = 0;
  if(Key != 0) Serial.println(Key);

  doorSwitch = (~digitalRead(DOOR_SW_PIN)) & 0x1; //invert, floats to 12V when light off, 0v when on
  //change to set flag in display_flags
//include fade timer and such somehow
  
  switch(Key){
    case INC_RED:   liveRed += incrementValue;
                    Serial.println("Red: ");
                    Serial.println(liveRed,DEC);
                    break;
    case INC_GREEN: liveGreen += incrementValue;
                    Serial.println("Green: ");
                    Serial.println(liveGreen,DEC);
                    break;
    case INC_BLUE:  liveBlue += incrementValue;
                    Serial.println("Blue: ");
                    Serial.println(liveBlue,DEC);
                    break;
    case OP_SAVE: savedRed   = liveRed;
                  savedGreen = liveGreen;
                  savedBlue   = liveBlue;
                  setRgbEeprom();
                  display_flags |= FLASH_LEDS; //let the display routine clear this
                  break;
    case OP_RESTORE: setRgbLedsSaved();
                     break;
    case OP_TOGGLE: if((display_flags & CTRL_ON) == CTRL_ON) display_flags &= ~CTRL_ON;
                    else                                     display_flags |= CTRL_ON;
                    break;
    case NO_OP:
    default: break; //do nothing
  }

  if(doorSwitch) {
    if((display_flags & DOOR_SW) == 0) display_flags |= FADE_ON;    //on now, was off then initiate fade on
    display_flags |= DOOR_SW;
  }else{
    if((display_flags & DOOR_SW) == DOOR_SW) display_flags |= FADE_OFF; //off now was on then fade off
    display_flags &= ~DOOR_SW;
  }
  
  
  for(int j = 0; j < 4; j++){
    control_leds();
    delay(20);
  }
}

#define PRESS_HOLD 4
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
  if(digitalRead(TOUCH_CTRL )) this_press |= OP_TOGGLE;

  if(this_press == 0){
    if((last_press < 5)||(last_press == OP_ABORT)) { //allow abort or last command to clear
      last_press = 0;
      return 0;
    }
     if((press_count > 0)&&(press_count < (LONG_PRESS - PRESS_HOLD))){
     this_press = last_press; /*assuming this is OP_TOGGLE*/
     last_press = 0; /*return that value */
     return this_press;
    }
  }
  if(this_press < 5){
    if((last_press == 0)||(last_press != this_press)){
      press_count = PRESS_HOLD; /*first touch of button, return the colour*/
      last_press = this_press;
      return 0;
    }
    /*if(last_press != this_press){
      last_press = OP_ABORT; //another button has been pressed too, wait for release
      return 0;
    }*/
    press_count--;
    if(press_count == 0){ /*or we've counted down, so increment colour by returning that value*/
      press_count = PRESS_HOLD;
      return this_press;
    }
  }
  if(this_press == OP_TOGGLE){
    if((last_press == 0)||(last_press != this_press)){
      press_count = LONG_PRESS; /*first touch of button, return the colour*/
      last_press = this_press;
      return 0;
    }
    press_count--;
    if(press_count == 0){ //if we've counted down here we will save
      press_count = 0;
      last_press = OP_ABORT; /*code will save and flash twice*/
      return OP_SAVE;
    }else{
      return 0; //wait for release to return toggle 
    }
  }
  if(this_press == OP_RESTORE){
    if((last_press == 0)||(last_press != this_press)){
      last_press = this_press;
      press_count = LONG_PRESS;
      return 0;
    }
    /*if(last_press != this_press){
      last_press = OP_ABORT; //another button has been pressed too, wait for release
      return 0;
    }*/
    press_count--;
    if(press_count == 0){
      last_press = OP_ABORT; /*timer cleared so send restore command*/
      return OP_RESTORE;
    }else{
      return 0;
    }
  }
  return 0;
}
