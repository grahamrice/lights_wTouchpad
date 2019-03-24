#include <EEPROM.h>

int led = 13;

/* Define the digital pins used for the clock and data */
#define SCL_PIN 8
#define SDO_PIN 9

#define DOOR_SW_PIN 2

#define RGB_RED 3
#define RGB_GREEN 5
#define RGB_BLUE 6

#define EEPROM_START_ADDR 0xC0

/* Used to store the key state */
byte Key;
byte lastReadScroll = 0;

int writeToEepromFlag = 0;
int restoreFromEepromFlag = 0;

int doorSwitch = 0;
byte switchFadeTimer = 0;

int savedRed = 0;
int savedGreen = 0;
int savedBlue = 0;
int liveRed, liveGreen, liveBlue;

/*-------------------------Set Leds----------------------------------*/

void setRgbLeds(int rIn, int gIn, int bIn)
{
  analogWrite(RGB_RED, rIn);
  analogWrite(RGB_GREEN, gIn);
  analogWrite(RGB_BLUE, bIn);
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
  setRgbLeds(savedRed, savedGreen, savedBlue);
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

  getRgbEeprom();

  //dumpEeprom();
}

/*-----------------------Dump EEPROM------------------------*/

void dumpEeprom()
{
  for(int e = 0; e < EEPROM.length(); e++){
    Serial.println(EEPROM[e],HEX);
  }
}

int determineLongPress(byte input)
{
  static int button1Counter = 0, button5Counter = 0;
  if(input == 1){
    button1Counter++;
  }
  if(input == 5){
    button5Counter++;
  }
  if(input == 0)
  {
    if(++button1Counter >= 4) 
    {
      button1Counter = 0;
      return 1;
    }
    if(++button5Counter >= 4)
    {
      button5Counter = 0;
      return 5;
    }
    button1Counter = 0;
    button5Counter = 0;
  }
  return 0;
}

#define incrementValue 0x1f

void processResult(byte input){
  static int onFlag = 0;
  static int Flag1 = 0, Flag5 = 0; /*flag indicates if button was pressed last time around to detect short press if now clear*/
  int dummy;
  switch(input){
    case 0: dummy = determineLongPress(input);
            if(dummy == 1){
              writeToEepromFlag = 1;
              savedRed = liveRed;    /*store them before saving them*/
              savedGreen = liveGreen;
              savedBlue = liveBlue;
              Flag1 = 0;
            }else if(dummy == 5){
              restoreFromEepromFlag = 1;
              Flag5 = 0;
            }else{
              if(Flag1) {
                Flag1 = 0;
                Serial.println("Short press on 1 detected");
                if(onFlag) break;
                liveRed = savedRed;       //switch on
                liveGreen = savedGreen;
                liveBlue = savedBlue;
                onFlag = 1;
              }
              if(Flag5){
                Flag5 = 0;
                savedRed = liveRed;   //switch off
                savedGreen = liveGreen;
                savedBlue = liveBlue;
                onFlag = 0;
                Serial.println("Short press on 5 detected");
              }
            }
            break;
    case 1: dummy = determineLongPress(input);
            if(dummy == 0) Flag1 = 1;
            break;
    case 2: liveRed += incrementValue;
            if(liveRed > 0xff) liveRed = 0xff;
            break;
    case 3: liveGreen += incrementValue;        
            if(liveGreen > 0xff) liveGreen = 0xff;
            break;
    case 4: liveBlue += incrementValue;
            if(liveBlue > 0xff) liveBlue = 0xff;
            break;
    case 5: dummy = determineLongPress(input);
            if(dummy == 0) Flag5 = 1;
            break;
    case 6: liveRed -= incrementValue;
            if(liveRed < 0x00) liveRed = 0x00;
            break;
    case 7: liveGreen -= incrementValue;
            if(liveGreen < 0x00) liveGreen = 0x00;
            break;
    case 8: liveBlue -= incrementValue;
            if(liveBlue < 0x00) liveBlue = 0x00;
            break;
    default: liveRed = liveRed;//do nothing
  }
  if(onFlag | doorSwitch | (switchFadeTimer != 0)){
    if(doorSwitch) switchFadeTimer = 50;
    else if(switchFadeTimer != 0) switchFadeTimer--;
    setRgbLeds(liveRed,liveGreen,liveBlue);
  }else{
    setRgbLedsOff();
  }
}

/* Main program */
void loop()
{
  /* Read the current state of the keypad */
  byte lastReadKey;
  Key = Read_Keypad();

  doorSwitch = (~digitalRead(DOOR_SW_PIN)) & 0x1; //invert, floats to 12V when light off, 0v when on
  
  /* If a key has been pressed output it to the serial port */
  processResult(Key);
  if (Key){
    Serial.println(Key);
    lastReadScroll = Key; 
    digitalWrite(led, HIGH);
     
  }
  else {
    lastReadScroll = 0;
    digitalWrite(led, LOW);
  }
  checkEepromFlags();
/*  if (lastReadKey != Key){
    setRgbLed(Key);
    lastReadKey = Key;
  }*/
  /* Wait a little before reading again 
     so not to flood the serial port*/
  delay(100);
}


/* Read the state of the keypad */
byte Read_Keypad(void)
{
  byte Count;
  byte Key_State = 0;
  byte touch = 0;

  /* Pulse the clock pin 16 times (one for each key of the keypad) 
     and read the state of the data pin on each pulse */
  for(Count = 1; Count <= 8; Count++)
  {
    digitalWrite(SCL_PIN, LOW); 
    
    /* If the data pin is low (active low mode) then store the 
       current key number */
    if (!digitalRead(SDO_PIN)){
      Key_State = Count;
      touch++;}
    
    digitalWrite(SCL_PIN, HIGH);

  }  
  
  return Key_State; 
}
