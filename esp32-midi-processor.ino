/*
    esp32 Midi Processor
*/
#include <HardwareSerial.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h> // https://github.com/madhephaestus/ESP32Encoder.git 
#include <usbh_midi.h>
#include <usbhub.h>
#include <AbleButtons.h>
#include "AppFeature.h"
#include <Fonts/FreeSans9pt7b.h>
#include <BlockNot.h>   

#define VERSION "0.30"

// Declaration for SSD1306 display connected using I2C
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUM_LEDS 5
#define LED_DATA_PIN 2
#define LED_OFF 0
#define LED_ON 10
Adafruit_NeoPixel pixels(NUM_LEDS, LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

// Serial
#define RXD2 16
#define TXD2 17
#define TXD1 4
#define RXD1 34

// Rotary
#define CLK 14 // CLK ENCODER 
#define ENC_C 27
#define DT 12 // DT ENCODER 
ESP32Encoder encoder;
int64_t iEncoderLast=0;

#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP 0xFC

// USB
USB Usb;
USBHub Hub(&Usb);
USBH_MIDI  Midi(&Usb);
#define MAX_RESET 15 //MAX3421E pin 12
#define MAX_GPX   13 //MAX3421E pin 17


using Button = AblePulldownClickerButton;

Button btnA(35);
Button btnB(32);
Button btnC(33);
Button btnD(25);
Button btnEnc(26);

BlockNot tmrIn1(50); //In Milliseconds 
BlockNot tmrIn2(50);
BlockNot tmrOut1(50);
BlockNot tmrOut2(50);
BlockNot tmrUSB(50);

bool bBtnA_Reset = false;
bool bBtnB_Reset = false;
bool bBtnC_Reset = false;
bool bBtnD_Reset = false;
bool bBtnEnc_Reset = false;

bool bBtnA_old = false;
bool bBtnB_old = false;
bool bBtnC_old = false;
bool bBtnD_old = false;
bool bBtnEnc_old = false;

bool btnA_Held = false;
bool btnB_Held = false;
bool btnC_Held = false;
bool btnD_Held = false;
bool btnEnc_Held = false;

uint8_t midiPacket_IN1[] = {0xFF, 0xFF, 0xFF};
uint8_t midiPacket_IN2[] = {0xFF, 0xFf, 0xFF};
uint8_t midiPacket_IN3[] = {0xFF, 0xFF, 0xFF};
uint8_t iCounter_IN1 = 0;
uint8_t iCounter_IN2 = 0;


AppFeature arrFeatures[] = {
  AppFeature("Route 1-x", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_NONE),
  AppFeature("Route 1-1", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_1, true),
  AppFeature("Route 1-2", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_2),
  AppFeature("Route 1-3", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_3),
  AppFeature("Route 1-1,2", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_12),
  AppFeature("Route 1-1,3", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_13),
  AppFeature("Route 1-2,3", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_23),
  AppFeature("Route 1-All", 0, FEATURE_GROUP_ROUTING_IN_1, ROUTING_TO_123),

  AppFeature("Route 2-x", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_NONE, true),
  AppFeature("Route 2-1", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_1),
  AppFeature("Route 2-2", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_2),
  AppFeature("Route 2-3", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_3),
  AppFeature("Route 2-1,2", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_12),
  AppFeature("Route 2-1,3", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_13),
  AppFeature("Route 2-2,3", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_23),
  AppFeature("Route 2-All", 0, FEATURE_GROUP_ROUTING_IN_2, ROUTING_TO_123),

  AppFeature("Route 3-x", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_TO_NONE, true),
  AppFeature("Route 3-1", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_TO_1),
  AppFeature("Route 3-2", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_TO_2),
  //AppFeature("Route 3-3", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_3_TO_3),
  AppFeature("Route 3-1,2", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_TO_12),
  //AppFeature("Route 3-1,3", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_3_TO_13),
  //AppFeature("Route 3-2,3", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_3_TO_23),
  //AppFeature("Route 3-All", 0, FEATURE_GROUP_ROUTING_IN_3, ROUTING_3_TO_123),

  AppFeature("1 Vel PT", 1, FEATURE_GROUP_VELOCITY, VELOCITY_PASSTHRU, true),
  AppFeature("1 Vel Fix63", 1, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_63),
  AppFeature("1 Vel Fix100", 1, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_100),
  AppFeature("1 Vel Fix127", 1, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_127),
  AppFeature("1 Vel Rnd", 1, FEATURE_GROUP_VELOCITY, VELOCITY_RANDOM),
  AppFeature("1 Vel Rnd100", 1, FEATURE_GROUP_VELOCITY, VELOCITY_RANDOM_100),
  
  AppFeature("1 Scale PT", 1, FEATURE_GROUP_SCALE, SCALE_PASSTHRU, true),
  AppFeature("1 Scale Maj", 1, FEATURE_GROUP_SCALE, SCALE_MAJOR),
  AppFeature("1 Scale Min", 1, FEATURE_GROUP_SCALE, SCALE_MINOR),
  AppFeature("1 Scale 5Maj", 1, FEATURE_GROUP_SCALE, SCALE_PENTATONIC_MAJOR),
  AppFeature("1 Scale 5Min", 1, FEATURE_GROUP_SCALE, SCALE_PENTATONIC_MINOR),

  AppFeature("1 Root None", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_PASSTHROUGH, true),
  AppFeature("1 Root C", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_C),
  AppFeature("1 Root C#", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Cs),
  AppFeature("1 Root D", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_D),
  AppFeature("1 Root D#", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Ds),
  AppFeature("1 Root E", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_E),
  AppFeature("1 Root F", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_F),
  AppFeature("1 Root F#", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Fs),
  AppFeature("1 Root G", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_G),
  AppFeature("1 Root G#", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Gs),
  AppFeature("1 Root A", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_A),
  AppFeature("1 Root A#", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_As),
  AppFeature("1 Root H", 1, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_H),

  //AppFeature("1 Fltr", 1, FEATURE_GROUP_SCALE_HANDLER, SCALE_HANDLER_FILTER, true),
  //AppFeature("1 Map", 1, FEATURE_GROUP_SCALE_HANDLER, SCALE_HANDLER_MAPPER),

  AppFeature("1 Note Ch PT", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_PASSTHRU, true),
  AppFeature("1 Note Ch 1", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_1),
  AppFeature("1 Note Ch 2", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_2),
  AppFeature("1 Note Ch 3", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_3),
  AppFeature("1 Note Ch 4", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_4),
  AppFeature("1 Note Ch 5", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_5),
  AppFeature("1 Note Ch 6", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_6),
  AppFeature("1 Note Ch 7", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_7),
  AppFeature("1 Note Ch 8", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_8),
  AppFeature("1 Note Ch 9", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_9),
  AppFeature("1 Note Ch 10", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_10),
  AppFeature("1 Note Ch 11", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_11),
  AppFeature("1 Note Ch 12", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_12),
  AppFeature("1 Note Ch 13", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_13),
  AppFeature("1 Note Ch 14", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_14),
  AppFeature("1 Note Ch 15", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_15),
  AppFeature("1 Note Ch 16", 1, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_16),

  AppFeature("1 CC Ch PT", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_PASSTHRU, true),
  AppFeature("1 CC Ch 1", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_1),
  AppFeature("1 CC Ch 2", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_2),
  AppFeature("1 CC Ch 3", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_3),
  AppFeature("1 CC Ch 4", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_4),
  AppFeature("1 CC Ch 5", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_5),
  AppFeature("1 CC Ch 6", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_6),
  AppFeature("1 CC Ch 7", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_7),
  AppFeature("1 CC Ch 8", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_8),
  AppFeature("1 CC Ch 9", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_9),
  AppFeature("1 CC Ch 10", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_10),
  AppFeature("1 CC Ch 11", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_11),
  AppFeature("1 CC Ch 12", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_12),
  AppFeature("1 CC Ch 13", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_13),
  AppFeature("1 CC Ch 14", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_14),
  AppFeature("1 CC Ch 15", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_15),
  AppFeature("1 CC Ch 16", 1, FEATURE_GROUP_CC_CHANNEL, CHANNEL_16),

  AppFeature("2 CC Ch PT", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_PASSTHRU, true),
  AppFeature("2 CC Ch 1", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_1),
  AppFeature("2 CC Ch 2", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_2),
  AppFeature("2 CC Ch 3", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_3),
  AppFeature("2 CC Ch 4", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_4),
  AppFeature("2 CC Ch 5", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_5),
  AppFeature("2 CC Ch 6", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_6),
  AppFeature("2 CC Ch 7", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_7),
  AppFeature("2 CC Ch 8", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_8),
  AppFeature("2 CC Ch 9", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_9),
  AppFeature("2 CC Ch 10", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_10),
  AppFeature("2 CC Ch 11", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_11),
  AppFeature("2 CC Ch 12", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_12),
  AppFeature("2 CC Ch 13", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_13),
  AppFeature("2 CC Ch 14", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_14),
  AppFeature("2 CC Ch 15", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_15),
  AppFeature("2 CC Ch 16", 2, FEATURE_GROUP_CC_CHANNEL, CHANNEL_16),
  AppFeature(" ", 0, FEATURE_GROUP_PLACEHOLDER, 0)
};

const uint8_t FEATURECOUNT = 95;
int iMenuPosition = -3;
uint8_t iRootNoteOffset=0;

void setup() {

  Button::setHeldTime(1500);
  //Button::setIdleTime(10000);
  btnA.begin();
  btnB.begin();
  btnC.begin();
  btnD.begin();
  btnEnc.begin();

  // initialize the OLED object
  //Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);       // Set text size
  display.setTextColor(WHITE);  // Set text color

  //Encoder
  pinMode(ENC_C, OUTPUT);
  digitalWrite(ENC_C, LOW);

  encoder.attachFullQuad(DT, CLK);
  encoder.setCount ( 0 );

  // For the USB connectin to the board itself, just use Serial as normal:
  Serial.begin(115200);
  Serial.print("Serial ready");

  pinMode(MAX_GPX, INPUT);
  pinMode(MAX_RESET, OUTPUT);
  digitalWrite(MAX_RESET, LOW);
  delay(50);
  digitalWrite(MAX_RESET, HIGH);
  delay(50);

  if (Usb.Init() == -1) {
    while (1); //halt
  }
  delay( 200 );

  Midi.attachOnInit(onInit);

  // Serial2 NACH usb.init()
  Serial1.begin(31250, SERIAL_8N1, RXD1, TXD1);
  Serial2.begin(31250, SERIAL_8N1, RXD2, TXD2);

  pixels.begin();
   for(uint8_t i=0; i<NUM_LEDS; i++){
    pixels.setPixelColor(i, pixels.Color(LED_OFF, LED_OFF, LED_OFF));
  }
  pixels.show();

  displayText("XXX32 Midi", "Firmware", "Firmware:", VERSION);
  delay(1000);
  processMenuNavigation(0);
}//Setup


typedef struct{
  uint8_t Velocity = VELOCITY_PASSTHRU;
  uint8_t NoteChannel = CHANNEL_PASSTHRU;
  uint8_t CCChannel = CHANNEL_PASSTHRU;
  uint8_t Scale = SCALE_PASSTHRU;
} OutputSettings;

OutputSettings output[3];


/*
Instead of repeated for..loops we are going to store the selected settings per output into 
a set of reusable varia
*/
void enumSetting(){
  for(uint8_t i=0; i<FEATURECOUNT; i++){
    if(arrFeatures[i].isSelected()){
    
    }
  }
}

void loop() {
  Usb.Task();
  if ( Midi ) {
    //MIDI_poll3();
    uint8_t size;
    do {
      // Idee: uint8_t lookupMsgSize(uint8_t midiMsg, uint8_t cin=0);
      //Serial.println("Poll:" ü );
      size = Midi.RecvData(midiPacket_IN3);
    } while (size > 0);
  }

  // encoder.attachFullQuad() provides bes results but counts 2 steps on one click, 
  // so we only count every 2nd step
  if(encoder.getCount()<iEncoderLast-1){
    processMenuNavigation( -1 );
    iEncoderLast=encoder.getCount();
  }
  
  if(encoder.getCount()>iEncoderLast+1){
    processMenuNavigation( +1 );
    iEncoderLast=encoder.getCount();
  }

  readData();

  checkButton_A();
  checkButton_B();
  checkButton_C();
  checkButton_D();
  checkButton_Enc();

  if (tmrIn1.FIRST_TRIGGER){ 
    pixels.setPixelColor(0, pixels.Color(LED_OFF, LED_OFF, LED_OFF));
    pixels.show();
  }  

  if (tmrIn2.FIRST_TRIGGER){ 
    pixels.setPixelColor(1, pixels.Color(LED_OFF, LED_OFF, LED_OFF));
    pixels.show();
  }  

  if (tmrOut1.FIRST_TRIGGER){ 
    pixels.setPixelColor(2, pixels.Color(LED_OFF, LED_OFF, LED_OFF));
    pixels.show();
  }  

  if (tmrOut2.FIRST_TRIGGER){ 
    pixels.setPixelColor(3, pixels.Color(LED_OFF, LED_OFF, LED_OFF));
    pixels.show();
  }  

  if (tmrUSB.FIRST_TRIGGER){ 
    pixels.setPixelColor(4, pixels.Color(LED_OFF, LED_OFF, LED_OFF));
    pixels.show();
  }  


}// Loop

/*
  Based on routing-configuration the input from Midi1, Midi2 and USB might be interesting
  this function makes sure that there's data in midiPacket_IN1, midiPacket_IN2 and midiPacket_IN3
  If no data were reeived, midiPacket_IN1 etc contain [0xFF, 0xFF, 0xFF] 
*/
void readData(){
  //Serial.println("readData()");
  for(uint8_t i=0; i<FEATURECOUNT; i++){
    if(arrFeatures[i].isSelected()){
      uint8_t iRoutingSelection;
      if(arrFeatures[i].getFeatureGroup()==FEATURE_GROUP_ROUTING_IN_1){
        iRoutingSelection = arrFeatures[i].getFeature();
        if( iRoutingSelection==ROUTING_TO_NONE ) {
          // nix
        }else{
          checkMidiIn_1();
          processData(1);
        }
      }
      if(arrFeatures[i].getFeatureGroup()==FEATURE_GROUP_ROUTING_IN_2){
        iRoutingSelection = arrFeatures[i].getFeature();
        if(arrFeatures[i].getFeature()==ROUTING_TO_NONE) {
          // nix
        }else{
          checkMidiIn_2();
          processData(2);
        }
      }
      if(arrFeatures[i].getFeatureGroup()==FEATURE_GROUP_ROUTING_IN_3){
        if(arrFeatures[i].getFeature()==ROUTING_TO_NONE) {
          // nix
        }else{
          checkMidiIn_USB();
          processData(3);
        }
      }
    }
  }
}

/*
  In order to allow individual settings per output (e.g. fixed velo on out1, random velo on out2) the 
  modulators are applied to a temporary copy of the input's packet. Since we can only output data
  consecutively we iterate through every input-routing to decide what to do with the incoming data saved in 
  midiPacket_IN1
*/
void processData(uint8_t pInput){
  uint8_t tmpPacket[3];
  if(pInput==1){
    if(midiPacket_IN1[2] != 0xFF){
      copyData(tmpPacket, midiPacket_IN1);  // Daten landen in tmpPacket
      processVelocity(tmpPacket); // Daten bleiben in in tmpPacket
      process_Note_Channel(tmpPacket);
      process_CC_Channel(tmpPacket);
      sendPacket(1, tmpPacket);
      midiPacket_IN1[2] = 0xFF; // prevent repeated sending; 0xFF doesnt hurt
    }
  }else if(pInput==2){
    if(midiPacket_IN2[2] != 0xFF){
      copyData(tmpPacket, midiPacket_IN2);  // Daten landen in tmpPacket
      processVelocity(tmpPacket); // Daten bleiben in in tmpPacket
      process_Note_Channel(tmpPacket);
      process_CC_Channel(tmpPacket);
      sendPacket(2, tmpPacket);
      midiPacket_IN2[2] = 0xFF; // prevent repeated sending; 0xFF doesnt hurt
    }
  }else if(pInput==3){
    if(midiPacket_IN3[2] != 0xFF){
      copyData(tmpPacket, midiPacket_IN3);
      processVelocity(tmpPacket);
      process_Note_Channel(tmpPacket);
      process_CC_Channel(tmpPacket);
      sendPacket(3, tmpPacket);
      midiPacket_IN3[2] = 0xFF;
    }
  }else{
    // wtf?
  }
}

void copyData(uint8_t *theArray, uint8_t pInPacket[]) {
  for (uint8_t i = 0; i < 3; i++) {
    theArray[i] = pInPacket[i];
  }
}

void processVelocity(uint8_t *midiPacket){
  for(uint8_t i=0; i<FEATURECOUNT; i++){
    if(arrFeatures[i].isSelected()){
      if(arrFeatures[i].getFeatureGroup()==FEATURE_GROUP_VELOCITY){
        uint8_t iStatus = midiPacket[0] & 0xF0;  
        if((iStatus==0x80) or (iStatus==0x90)){ // Nur midi noten
          //Serial.println("processVelocity()");
          if(arrFeatures[i].getFeature()==VELOCITY_PASSTHRU) {
            //nix
          }
          if(arrFeatures[i].getFeature()==VELOCITY_FIX_63) {
            midiPacket[2] = 63;
          }
          if(arrFeatures[i].getFeature()==VELOCITY_FIX_100) {
            midiPacket[2] = 100;
          }
          if(arrFeatures[i].getFeature()==VELOCITY_FIX_127) {
            midiPacket[2] = 127;
          }
          if(arrFeatures[i].getFeature()==VELOCITY_RANDOM) {
            uint8_t rnd=random(128);
            midiPacket[2] = rnd;
          }
          if(arrFeatures[i].getFeature()==VELOCITY_RANDOM_100) {
            uint8_t rnd=random(20);
            midiPacket[2] = 91 + rnd;
          }
        }
      }
    }
  }
}

void process_Note_Channel(uint8_t *midiPacket){
  for(uint8_t i=0; i<FEATURECOUNT; i++){
    if(arrFeatures[i].isSelected()){
      if(arrFeatures[i].getFeatureGroup()==FEATURE_GROUP_NOTE_CHANNEL){
        uint8_t iStatus = midiPacket[0] & 0xF0;  
        if((iStatus==0x80) or (iStatus==0x90)){ // Nur midi noten
          //Serial.println("process_Note_Channel()");
          if(arrFeatures[i].getFeature()==CHANNEL_PASSTHRU) {
              //nix
          }else{
            midiPacket[0] = (midiPacket[0] & 0xF0) + arrFeatures[i].getFeature()-1;
          }
        }
      }
    }
  }
}

void process_CC_Channel(uint8_t *midiPacket){
  for(uint8_t i=0; i<FEATURECOUNT; i++){
    if(arrFeatures[i].isSelected()){
      if(arrFeatures[i].getFeatureGroup()==FEATURE_GROUP_CC_CHANNEL){
        uint8_t iStatus = midiPacket[0] & 0xF0;  
        if(iStatus==0xB0) { // Nur midi CCs
          //Serial.println("process_CC_Channel()");
          if(arrFeatures[i].getFeature()==CHANNEL_PASSTHRU) {
              //nix
          }else{
            midiPacket[0] = (midiPacket[0] & 0xF0) + arrFeatures[i].getFeature()-1;
          }
        }
      }
    }
  }
}


void sendPacket(uint8_t pInFrom, uint8_t *midiPacket){
  if(midiPacket[2]==0xFF){
    return;
  }
  for(uint8_t i=0; i<FEATURECOUNT; i++){
    if(arrFeatures[i].isSelected()){
      uint8_t iFeatureGroup;
      if(pInFrom==1){
        iFeatureGroup = FEATURE_GROUP_ROUTING_IN_1;
      }else if(pInFrom==2){
        iFeatureGroup = FEATURE_GROUP_ROUTING_IN_2;
      }else if(pInFrom==3){
        iFeatureGroup = FEATURE_GROUP_ROUTING_IN_3;
      } 
      if(arrFeatures[i].getFeatureGroup()==iFeatureGroup){
        if(arrFeatures[i].getFeature()==ROUTING_TO_NONE){
          //nix
        }
        uint8_t iRoutingTarget = arrFeatures[i].getFeature();
        if(iRoutingTarget==ROUTING_TO_1){
          //Serial.println("processRouting: Input " + String(pInFrom) + " to out 1");
          flashLED(1);
          Serial1.write(midiPacket, 3);
        }else if(iRoutingTarget==ROUTING_TO_2){
          //Serial.println("processRouting: Input " + String(pInFrom) + " to out 2");
          Serial2.write(midiPacket, 3);
          flashLED(2);
        }else if(iRoutingTarget==ROUTING_TO_3){
          //Serial.println("processRouting: Input " + String(pInFrom) + " to out 3");
          flashLED(3);
        }else if(iRoutingTarget==ROUTING_TO_12){
          //Serial.println("processRouting: Input " + String(pInFrom) + " to out 1+2");
          flashLED(1);
          Serial1.write(midiPacket, 3);
          flashLED(2);
          Serial2.write(midiPacket, 3);
        }else if(iRoutingTarget==ROUTING_TO_13){
          Serial.println("processRouting TO BE DONE: Input " + String(pInFrom) + " to out 1+3");
          flashLED(1);
          flashLED(3);
        }else if(iRoutingTarget==ROUTING_TO_23){
          Serial.println("processRouting TO BE DONE: Input " + String(pInFrom) + " to out 2+3");
          flashLED(2);
          flashLED(3);
        }else if(iRoutingTarget==ROUTING_TO_123){
          Serial.println("processRouting TO BE DONE: Input " + String(pInFrom) + " to out 1+2+3");
          flashLED(1);
          flashLED(2);
          flashLED(3);
        }
      }
    }
  }
}

void flashLED(uint8_t pOutport){
  pixels.setPixelColor(pOutport+1, pixels.Color(LED_ON, LED_OFF, LED_ON));
  pixels.show();
  if(pOutport==1){
    tmrOut1.RESET;
  }else if(pOutport==2){
    tmrOut2.RESET;
  }else if(pOutport==3){
    tmrUSB.RESET;
  }
}


void checkButton_A(){
   btnA.handle();
  // bBtnA_old: Damit isHeld() nur einmal getriggert wird.
  //
  if(btnA.isClicked()){
    if(bBtnA_Reset==false){
      if(btnA_Held==false){
        Serial.println("BtnA isClicked");
        displayText("ASSA ASSA", "ANDY", "MUSIK:", "COOL");
      }
      btnA.resetClicked();
    }
    bBtnA_Reset = false;
    btnA_Held=false;
  }

  if(btnA.isHeld()){
    if(!bBtnA_old){
      Serial.println("BtnA isHeld");
      bBtnA_old=true;
      btnA_Held = true;
      bBtnA_Reset=btnA.resetClicked();
    }
  }else{
    bBtnA_old=false;
  }
}

void checkButton_B(){
   btnB.handle();
  if(btnB.isClicked()){
    if(bBtnB_Reset==false){
      if(btnB_Held==false){
        Serial.println("BtnB isClicked");
      }
      btnB.resetClicked();
    }
    bBtnB_Reset = false;
    btnB_Held=false;
  }

  if(btnB.isHeld()){
    if(!bBtnB_old){
      Serial.println("BtnB isHeld");
      bBtnB_old=true;
      btnB_Held = true;
      bBtnB_Reset=btnB.resetClicked();
    }
  }else{
    bBtnB_old=false;
  }
}

void checkButton_C(){
  btnC.handle();
  if(btnC.isClicked()){
    if(bBtnC_Reset==false){
      if(btnC_Held==false){
        Serial.println("BtnC isClicked");
      }
      btnC.resetClicked();
    }
    bBtnC_Reset = false;
    btnC_Held=false;
  }

  if(btnC.isHeld()){
    if(!bBtnC_old){
      Serial.println("BtnC isHeld");
      bBtnC_old=true;
      btnC_Held = true;
      bBtnC_Reset=btnC.resetClicked();
    }
  }else{
    bBtnC_old=false;
  }
}

void checkButton_D(){
  btnD.handle();
  if(btnD.isClicked()){
    if(bBtnD_Reset==false){
      if(btnD_Held==false){
        Serial.println("BtnD isClicked");
      }
      btnD.resetClicked();
    }
    bBtnD_Reset = false;
    btnD_Held=false;
  }

  if(btnD.isHeld()){
    if(!bBtnD_old){
      Serial.println("BtnD isHeld");
      bBtnD_old=true;
      btnD_Held = true;
      bBtnD_Reset=btnD.resetClicked();
    }
  }else{
    bBtnD_old=false;
  }
}

void checkButton_Enc(){
  btnEnc.handle();
  if(btnEnc.isClicked()){
    if(bBtnEnc_Reset==false){
      if(btnEnc_Held==false){
        processEncoderClick();
      }
      btnEnc.resetClicked();
    }
    bBtnEnc_Reset = false;
    btnEnc_Held=false;
  }

  if(btnEnc.isHeld()){
    if(!bBtnEnc_old){
      Serial.println("BtnEnc isHeld");
      bBtnEnc_old=true;
      btnEnc_Held = true;
      bBtnEnc_Reset=btnEnc.resetClicked();
    }
  }else{
    bBtnEnc_old=false;
  }
}

// Stores up to(!) 3 bytes into midiPacket_IN1
void checkMidiIn_1(){ 
  while ((Serial1.available() > 0) && (iCounter_IN1 < 3)){
    pixels.setPixelColor(0, pixels.Color(LED_ON, LED_ON, LED_OFF));
    pixels.show();
    tmrIn1.RESET;
    byte tmp = Serial1.read();
    midiPacket_IN1[iCounter_IN1] = tmp;
    //Serial.print(midiPacket_IN1[iCounter_IN1]); Serial.print(" ");
    if((tmp==MIDI_START)||(tmp==MIDI_STOP)||(tmp==MIDI_CLOCK)){
      iCounter_IN1=3;
      midiPacket_IN1[2] = 0x00; //inbound signaling ... worst idea ever. however this way we can also deal with midi clock data.
    }
    iCounter_IN1++;
    if(iCounter_IN1==3){
      iCounter_IN1=0;
    }
  }
}


void checkMidiIn_2(){ 
  while ((Serial2.available() > 0) && (iCounter_IN2 < 3)){
    pixels.setPixelColor(1, pixels.Color(LED_ON, LED_ON, LED_OFF));
    pixels.show();
    tmrIn2.RESET;
    byte tmp = Serial2.read();
    midiPacket_IN2[iCounter_IN2] = tmp;
    //Serial.print(midiPacket_IN1[iCounter_IN1]); Serial.print(" ");
    if((tmp==MIDI_START)||(tmp==MIDI_STOP)||(tmp==MIDI_CLOCK)){
      iCounter_IN2=3;
      midiPacket_IN2[2] = 0x00; //inbound signaling ... worst idea ever. however this way we can also deal with midi clock data.
    }
    iCounter_IN2++;
    if(iCounter_IN2==3){
      iCounter_IN2=0;
    }
  }
}

// USB is handled differently
void checkMidiIn_USB(){
  if(midiPacket_IN3[2]!=0xFF){
    pixels.setPixelColor(4, pixels.Color(LED_ON, LED_ON, LED_OFF));
    pixels.show();
    tmrUSB.RESET;
  }
}

 
void onInit()
{
  char buf[20];
  uint16_t vid = Midi.idVendor();
  uint16_t pid = Midi.idProduct();
  sprintf(buf, "VID:%04X, PID:%04X", vid, pid);
  Serial.println(buf); 
}

void displayText(String pLine1, String pLine2, String pLine3, String pLine4){
  display.clearDisplay();
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);


  display.setCursor(0, 12);
  display.println( (pLine1) );

  display.setCursor(0, 29);
  display.println( pLine2 );

  display.setCursor(0, 46);
  display.println( pLine3 );

  display.setCursor(0, 63);
  display.println( pLine4);
  display.display();
}

void processMenuNavigation(int pDirection){
  if(iMenuPosition==-3){  // on the fery first encoder rotation we jump to the first item
    iMenuPosition = 0;
  }else{
    iMenuPosition += pDirection;
  }
 
  if(iMenuPosition<0){
    iMenuPosition = FEATURECOUNT-1;
  }else if(iMenuPosition>=FEATURECOUNT){
    iMenuPosition =0;
  }
  displayText(getMenuItem( iMenuPosition ), getMenuItem( iMenuPosition+1 ), getMenuItem( iMenuPosition+2 ) , "");
}


String getMenuItem(int pPosition){
  String sTmp = arrFeatures[pPosition].getText();
  if(arrFeatures[pPosition].isSelected()){
    sTmp += " *";
  }
  return getFeaturePrefix(pPosition) + " " + sTmp;
}

String getFeaturePrefix(uint8_t pIndex){
  if(arrFeatures[pIndex].getFeatureGroup()==FEATURE_GROUP_NOTE_CHANNEL){
    //return "Ch";
    return "";
  }else if(arrFeatures[pIndex].getFeatureGroup()==FEATURE_GROUP_VELOCITY){
    //return "Vel";
    return "";
  }if(arrFeatures[pIndex].getFeatureGroup()==FEATURE_GROUP_SCALE){
    //return "Scale";
    return "";  //because 'Major, Minor, etc is unique by itself and saves string data
  }if(arrFeatures[pIndex].getFeatureGroup()==FEATURE_GROUP_ROOTNOTE){
    //return "Root";
    return "";
  }if(arrFeatures[pIndex].getFeatureGroup()==FEATURE_GROUP_SCALE_HANDLER){
    return "";
  }else{
    return "";
  }
}

void processEncoderClick(){
  // We are at iMenuPosition
  uint8_t tmpFG = arrFeatures[iMenuPosition].getFeatureGroup();
  for(uint8_t i=0; i< FEATURECOUNT; i++){
    if( arrFeatures[i].getFeatureGroup() == tmpFG){
      arrFeatures[i].select(false);
    }
  }

  // If scale passthrough is selected then we select root-note-passthrough.
  if(arrFeatures[iMenuPosition].getFeatureGroup()==FEATURE_GROUP_SCALE){
    if(arrFeatures[iMenuPosition].getFeature()==SCALE_PASSTHRU){
      for(uint8_t i=0; i< FEATURECOUNT; i++){
        if( arrFeatures[i].getFeatureGroup() == FEATURE_GROUP_ROOTNOTE){
          if(arrFeatures[i].getFeature()==ROOTNOTE_PASSTHROUGH){
            arrFeatures[i].select(true);
          }else{
            arrFeatures[i].select(false);
          }
        }
      }
    }
  }

  if(  arrFeatures[iMenuPosition].getFeatureGroup() != FEATURE_GROUP_PLACEHOLDER ){
    arrFeatures[iMenuPosition].select(true);
  }
  
  if(arrFeatures[iMenuPosition].getFeatureGroup()==FEATURE_GROUP_ROOTNOTE){
    if(arrFeatures[iMenuPosition].getFeature() > ROOTNOTE_PASSTHROUGH){
      iRootNoteOffset = arrFeatures[iMenuPosition].getFeature()-1; // <-- megapfiffig!
    }
  }
  
  displayText(getMenuItem( iMenuPosition ), getMenuItem( iMenuPosition+1 ), getMenuItem( iMenuPosition+2 ) , "");
}
