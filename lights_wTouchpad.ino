#include <EEPROM.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

int led = 13;

#define DOORSW_USEANALOGUE

#define DOOR_SW_PIN  2
#define DOOR_SW_PIN_A A0
#define IGNITION_PIN 4

#define RGB_RED 11 /* was 9 now 11 */
#define RGB_GREEN 5
#define RGB_BLUE 6

#define TOUCH_CTRL   3
#define TOUCH_RED   10 
#define TOUCH_GREEN  9 /* was 11 now 9*/
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
#define TIMED_OFF  0x40
#define UPDATE     0x80
#define RES_TIMER  0xFD /*inverse of CTRL_ON*/
short display_flags = 0;
short display_fade = 0;
short display_flash = 0;

#define TIMEOUT_DEF 4000
int timeout = TIMEOUT_DEF;

int savedRed = 0;
int savedGreen = 0;
int savedBlue = 0;
int liveRed, liveGreen, liveBlue;

#define PAUSE_DEF 3
int pause_red, pause_green, pause_blue;

byte gbl_key;

void periodic_display();
#define TIMER_RESET 60536

//-----------------commands--------------------
#define NO_OP      0
#define INC_RED    1
#define INC_GREEN  2
#define INC_BLUE   4
#define OP_TOGGLE  8
#define OP_SAVE    15
#define OP_RESTORE 5
#define OP_ABORT   255

/*--------------Sleep and return from sleep--------------------------*/

void returnfromsleep()
{
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(DOOR_SW_PIN));
  detachInterrupt(digitalPinToInterrupt(TOUCH_CTRL));
  timeout = TIMEOUT_DEF;
  display_flags &= ~TIMED_OFF;
}

void sleep()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(digitalPinToInterrupt(DOOR_SW_PIN),returnfromsleep,LOW);
  attachInterrupt(digitalPinToInterrupt(TOUCH_CTRL),returnfromsleep,LOW);
  sleep_mode();
}

/*------------------Timed interrupt to update display----------------*/

ISR(TIMER1_OVF_vect)
{
  TCNT1 = TIMER_RESET;
  periodic_display(); 
}
/*-------------------------Set Leds----------------------------------*/

void setRgbLeds(byte f)
{
  int rr,gg,bb;
  if(f == 0xff){
    analogWrite(RGB_RED, liveRed);
    analogWrite(RGB_GREEN, liveGreen);
    analogWrite(RGB_BLUE, liveBlue);
  }else{
    rr = (liveRed * f) >> 8;
    gg = (liveGreen * f) >> 8;
    bb = (liveBlue * f) >> 8;
    analogWrite(RGB_RED, rr & 0xff);
    analogWrite(RGB_GREEN, gg & 0xff);
    analogWrite(RGB_BLUE, bb & 0xff);
  }
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
    *value = val1 & 0xf0;
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
  setRgbLedsSaved(); //on start-up load from eeprom
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

/*------------------Setup-------------------------------*/
void setup()
{
  /* Initialise the serial interface */
  noInterrupts();
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

  pinMode(DOOR_SW_PIN,INPUT);
  /*pinMode(IGNITION_PIN,INPUT);*/

  pause_red = PAUSE_DEF;
  pause_green = PAUSE_DEF;
  pause_blue = PAUSE_DEF;
  
  getRgbEeprom();
  
  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = TIMER_RESET;      //for 80ms overflow = 5000*256/16MHz
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  
  interrupts();
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
  /*if((display_flags & TIMED_OFF) == TIMED_OFF){
    setRgbLedsOff();
    return;
  }*/
  
  if((display_flags & UPDATE) == 0) return; //only change pwm values if there is a reason to. Time changing modes do not update this

  if((display_flags & FLASH_LEDS) == FLASH_LEDS){ //highest priority
    if(display_flash == 0) display_flash = 12;
    display_flash--;
    if(display_flash > 8) {
      setRgbLedsOff();
    }else if(display_flash > 4) {
      setRgbLeds(0xff);
    }else if(display_flash >  0) {
      setRgbLedsOff();
    }else if(display_flash <= 0) {
      setRgbLeds(0xff);
      display_flags &= ~FLASH_LEDS;
      display_flags &= ~UPDATE;
      display_flash = 0;
    }
  
   return;  
  }
  if((display_flags & CTRL_ON) == CTRL_ON){
    setRgbLeds(0xff);
    display_flags &= ~UPDATE;
    return;
  }
  if((display_flags & FADE_ON) == FADE_ON){
    if(display_fade == 0xff) display_fade = 0x00; //chances are this will not happen
    display_fade += 16;
    if(display_fade > 0xff) {
      display_fade = 0xff;
      display_flags &= ~FADE_ON;
      display_flags &= ~UPDATE; /*only turn off update on completion*/
    }
    setRgbLeds(display_fade);
    
    return;
  }
  if((display_flags & FADE_OFF) == FADE_OFF){
    if(display_fade == 0) display_fade = 0xff;
    display_fade -= 16;
    if(display_fade < 0) {
      display_fade = 0;
      display_flags &= ~FADE_OFF;
      display_flags &= ~UPDATE;   /*only turn off update on completion*/
    }
    setRgbLeds(display_fade);
    
    return;
  }
  if((display_flags & DOOR_SW) == DOOR_SW){
    setRgbLeds(0xff);
    display_flags &= ~UPDATE;
    return;
  }

  display_flags &= ~UPDATE;
  setRgbLedsOff();
  return;                                         //lowest priority
}

#define incrementValue 0x10

/* fyi this function does not determine if we will sleep or not */
void periodic_display()
{
  byte doorSwitch;
  byte ignition = 0;
  
  doorSwitch = (~digitalRead(DOOR_SW_PIN)) & 0x1; //invert, floats to 12V when light off, 0v when on
  ignition   = (digitalRead(IGNITION_PIN));       /* don't let timer timeout if ignition is on*/
  
  switch(gbl_key){
    case INC_RED:   if(liveRed < incrementValue){
                      if(pause_red != 0) pause_red--;
                      else liveRed += incrementValue;
                    }else{
                      pause_red = PAUSE_DEF;
                      liveRed += incrementValue;
                    }
                    if(liveRed > 0xf8) liveRed = 0;
                    Serial.println("Red: ");
                    Serial.println(liveRed,DEC);
                    break;
    case INC_GREEN: if(liveGreen < incrementValue){
                      if(pause_green != 0) pause_green--;
                      else liveGreen += incrementValue;
                    }else{
                      pause_green = PAUSE_DEF;
                      liveGreen += incrementValue;
                    }     
                    if(liveGreen > 0xf8) liveGreen = 0;               
                    Serial.println("Green: ");
                    Serial.println(liveGreen,DEC);
                    break;
    case INC_BLUE:  if(liveBlue < incrementValue){
                      if(pause_blue != 0) pause_blue--;
                      else liveBlue += incrementValue;
                    }else{
                      pause_blue = PAUSE_DEF;
                      liveBlue += incrementValue;
                    }
                    if(liveBlue > 0xF8) liveBlue = 0;
                    Serial.println("Blue: ");
                    Serial.println(liveBlue,DEC);
                    break;
    case OP_SAVE: savedRed   = (byte)liveRed;
                  savedGreen = (byte)liveGreen;
                  savedBlue  = (byte)liveBlue;
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

  
  if(gbl_key != 0) {
    Serial.println("Flags:");
    Serial.println(display_flags,HEX);
  }
  
  if(doorSwitch) {
    if((display_flags & DOOR_SW) == 0) {
      if((display_flags & CTRL_ON) == 0) display_flags |= FADE_ON;    //on now, was off then initiate fade on
      display_flags |= UPDATE;
      Serial.println(display_flags,HEX);
      Serial.println(timeout);
    }
    display_flags |= DOOR_SW;
  }else{
    if((display_flags & DOOR_SW) == DOOR_SW) {
      if((display_flags & CTRL_ON) == 0) display_flags |= FADE_OFF; //off now was on then fade off
      display_flags |= UPDATE;
      Serial.println(display_flags,HEX);
      Serial.println(timeout);
    }
    display_flags &= ~DOOR_SW;
  }

  if((display_flags & RES_TIMER) != 0){
    timeout = TIMEOUT_DEF; /*if things are happening, hold the timer off*/
     /*if((display_flags & TIMED_OFF) == TIMED_OFF) display_flags &= ~TIMED_OFF;*/ /*make sure the power off flag is clear*/
  }
  else if(((display_flags & CTRL_ON) == CTRL_ON)&&(ignition == 0))       /* else if we're only in user-on mode*/
    {
      timeout--;                          /*count down*/
      if((timeout == 1000)||(timeout == 2000)) {
        Serial.print("Timing out in ");
        Serial.println(timeout);
      }
      if(timeout <= 0) {
        Serial.println("Going to sleep");
        display_flags &= ~CTRL_ON;    /* set flag to turn off*/
        display_flags |= UPDATE;     /*update display to turn it off*/
        display_flags |= TIMED_OFF;
      }
    }
  
  control_leds();

  
  gbl_key = 0;
}

/*------------------ Main program --------------------------*/
void loop()
{
  /* Read the current state of the keypad */
  byte lastReadKey;
  byte Key = Read_Keypad();
  
  if(Key != 0){
    Serial.println("Key:");
    Serial.println(Key);
    display_flags |= UPDATE;
  }

  gbl_key |= Key;

  delay(80);           /*timed flag not necessary as we can just turn it off at timeout. Maybe reintroduce when sleeping*/
  if((display_flags & TIMED_OFF) == TIMED_OFF) sleep(); //and add an interrupt for door sw and touch ctrl to turn back on*/
}

#define PRESS_HOLD 3
#define LONG_PRESS 10

/* Read the state of the keypad */
byte Read_Keypad(void)
{
  static byte last_press = 0;
  static short press_count = PRESS_HOLD;
  byte this_press = 0;

  if(1 & ~digitalRead(TOUCH_RED  )) this_press |= INC_RED;
  if(1 & ~digitalRead(TOUCH_GREEN)) this_press |= INC_GREEN;
  if(1 & ~digitalRead(TOUCH_BLUE )) this_press |= INC_BLUE;
  if(1 & ~digitalRead(TOUCH_CTRL )) this_press |= OP_TOGGLE;

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
