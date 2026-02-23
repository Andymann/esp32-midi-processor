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
#include <Fonts/FreeSans24pt7b.h>
#include <BlockNot.h>
#include <EEPROM.h>

#define VERSION "0.96"

#define PRESET_COUNT 4
#define EEPROM_SIZE  (2 + PRESET_COUNT * (1 + sizeof(AppSettings)))

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
BlockNot tmrDisplay(3000);

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

// Input packet with data and drop flag (replaces using 3rd byte 0xFF as sentinel)
struct MidiPacket {
  uint8_t data[3];
  bool drop;  // true = no data / do not send (filtered or already consumed)
};

// Queue: up to 10 incoming MIDI events, processed in order (FIFO)
#define MIDI_QUEUE_SIZE 10
struct QueuedMidiEvent {
  MidiPacket pkt;
  uint8_t source;  // 1, 2, or 3 for input 1, 2, USB
};
static QueuedMidiEvent midiQueue[MIDI_QUEUE_SIZE];
static uint8_t queueHead = 0;  // next write
static uint8_t queueTail = 0;  // next read
static uint8_t queueCount = 0;

static bool pushMidiQueue(const MidiPacket *pkt, uint8_t source) {
  if (queueCount >= MIDI_QUEUE_SIZE) return false;
  midiQueue[queueHead].pkt = *pkt;
  midiQueue[queueHead].pkt.drop = false;
  midiQueue[queueHead].source = source;
  queueHead = (queueHead + 1) % MIDI_QUEUE_SIZE;
  queueCount++;
  return true;
}
static bool popMidiQueue(QueuedMidiEvent *out) {
  if (queueCount == 0) return false;
  *out = midiQueue[queueTail];
  queueTail = (queueTail + 1) % MIDI_QUEUE_SIZE;
  queueCount--;
  return true;
}

MidiPacket midiPacket_IN1 = { {0xFF, 0xFF, 0xFF}, true };
MidiPacket midiPacket_IN2 = { {0xFF, 0xFF, 0xFF}, true };
MidiPacket midiPacket_IN3 = { {0xFF, 0xFF, 0xFF}, true };
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

  AppFeature("2 Vel PT", 2, FEATURE_GROUP_VELOCITY, VELOCITY_PASSTHRU, true),
  AppFeature("2 Vel Fix63", 2, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_63),
  AppFeature("2 Vel Fix100", 2, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_100),
  AppFeature("2 Vel Fix127", 2, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_127),
  AppFeature("2 Vel Rnd", 2, FEATURE_GROUP_VELOCITY, VELOCITY_RANDOM),
  AppFeature("2 Vel Rnd100", 2, FEATURE_GROUP_VELOCITY, VELOCITY_RANDOM_100),

  AppFeature("2 Scale PT", 2, FEATURE_GROUP_SCALE, SCALE_PASSTHRU, true),
  AppFeature("2 Scale Maj", 2, FEATURE_GROUP_SCALE, SCALE_MAJOR),
  AppFeature("2 Scale Min", 2, FEATURE_GROUP_SCALE, SCALE_MINOR),
  AppFeature("2 Scale 5Maj", 2, FEATURE_GROUP_SCALE, SCALE_PENTATONIC_MAJOR),
  AppFeature("2 Scale 5Min", 2, FEATURE_GROUP_SCALE, SCALE_PENTATONIC_MINOR),

  AppFeature("2 Root None", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_PASSTHROUGH, true),
  AppFeature("2 Root C", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_C),
  AppFeature("2 Root C#", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Cs),
  AppFeature("2 Root D", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_D),
  AppFeature("2 Root D#", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Ds),
  AppFeature("2 Root E", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_E),
  AppFeature("2 Root F", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_F),
  AppFeature("2 Root F#", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Fs),
  AppFeature("2 Root G", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_G),
  AppFeature("2 Root G#", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Gs),
  AppFeature("2 Root A", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_A),
  AppFeature("2 Root A#", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_As),
  AppFeature("2 Root H", 2, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_H),

  AppFeature("2 Note Ch PT", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_PASSTHRU, true),
  AppFeature("2 Note Ch 1", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_1),
  AppFeature("2 Note Ch 2", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_2),
  AppFeature("2 Note Ch 3", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_3),
  AppFeature("2 Note Ch 4", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_4),
  AppFeature("2 Note Ch 5", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_5),
  AppFeature("2 Note Ch 6", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_6),
  AppFeature("2 Note Ch 7", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_7),
  AppFeature("2 Note Ch 8", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_8),
  AppFeature("2 Note Ch 9", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_9),
  AppFeature("2 Note Ch 10", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_10),
  AppFeature("2 Note Ch 11", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_11),
  AppFeature("2 Note Ch 12", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_12),
  AppFeature("2 Note Ch 13", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_13),
  AppFeature("2 Note Ch 14", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_14),
  AppFeature("2 Note Ch 15", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_15),
  AppFeature("2 Note Ch 16", 2, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_16),

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

  AppFeature("3 Vel PT", 3, FEATURE_GROUP_VELOCITY, VELOCITY_PASSTHRU, true),
  AppFeature("3 Vel Fix63", 3, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_63),
  AppFeature("3 Vel Fix100", 3, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_100),
  AppFeature("3 Vel Fix127", 3, FEATURE_GROUP_VELOCITY, VELOCITY_FIX_127),
  AppFeature("3 Vel Rnd", 3, FEATURE_GROUP_VELOCITY, VELOCITY_RANDOM),
  AppFeature("3 Vel Rnd100", 3, FEATURE_GROUP_VELOCITY, VELOCITY_RANDOM_100),

  AppFeature("3 Scale PT", 3, FEATURE_GROUP_SCALE, SCALE_PASSTHRU, true),
  AppFeature("3 Scale Maj", 3, FEATURE_GROUP_SCALE, SCALE_MAJOR),
  AppFeature("3 Scale Min", 3, FEATURE_GROUP_SCALE, SCALE_MINOR),
  AppFeature("3 Scale 5Maj", 3, FEATURE_GROUP_SCALE, SCALE_PENTATONIC_MAJOR),
  AppFeature("3 Scale 5Min", 3, FEATURE_GROUP_SCALE, SCALE_PENTATONIC_MINOR),

  AppFeature("3 Root None", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_PASSTHROUGH, true),
  AppFeature("3 Root C", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_C),
  AppFeature("3 Root C#", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Cs),
  AppFeature("3 Root D", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_D),
  AppFeature("3 Root D#", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Ds),
  AppFeature("3 Root E", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_E),
  AppFeature("3 Root F", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_F),
  AppFeature("3 Root F#", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Fs),
  AppFeature("3 Root G", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_G),
  AppFeature("3 Root G#", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_Gs),
  AppFeature("3 Root A", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_A),
  AppFeature("3 Root A#", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_As),
  AppFeature("3 Root H", 3, FEATURE_GROUP_ROOTNOTE, ROOTNOTE_H),

  AppFeature("3 Note Ch PT", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_PASSTHRU, true),
  AppFeature("3 Note Ch 1", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_1),
  AppFeature("3 Note Ch 2", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_2),
  AppFeature("3 Note Ch 3", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_3),
  AppFeature("3 Note Ch 4", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_4),
  AppFeature("3 Note Ch 5", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_5),
  AppFeature("3 Note Ch 6", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_6),
  AppFeature("3 Note Ch 7", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_7),
  AppFeature("3 Note Ch 8", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_8),
  AppFeature("3 Note Ch 9", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_9),
  AppFeature("3 Note Ch 10", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_10),
  AppFeature("3 Note Ch 11", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_11),
  AppFeature("3 Note Ch 12", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_12),
  AppFeature("3 Note Ch 13", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_13),
  AppFeature("3 Note Ch 14", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_14),
  AppFeature("3 Note Ch 15", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_15),
  AppFeature("3 Note Ch 16", 3, FEATURE_GROUP_NOTE_CHANNEL, CHANNEL_16),

  AppFeature("3 CC Ch PT", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_PASSTHRU, true),
  AppFeature("3 CC Ch 1", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_1),
  AppFeature("3 CC Ch 2", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_2),
  AppFeature("3 CC Ch 3", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_3),
  AppFeature("3 CC Ch 4", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_4),
  AppFeature("3 CC Ch 5", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_5),
  AppFeature("3 CC Ch 6", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_6),
  AppFeature("3 CC Ch 7", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_7),
  AppFeature("3 CC Ch 8", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_8),
  AppFeature("3 CC Ch 9", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_9),
  AppFeature("3 CC Ch 10", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_10),
  AppFeature("3 CC Ch 11", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_11),
  AppFeature("3 CC Ch 12", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_12),
  AppFeature("3 CC Ch 13", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_13),
  AppFeature("3 CC Ch 14", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_14),
  AppFeature("3 CC Ch 15", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_15),
  AppFeature("3 CC Ch 16", 3, FEATURE_GROUP_CC_CHANNEL, CHANNEL_16),

  AppFeature("1 Block None", 1, FEATURE_GROUP_FILTER, BLOCK_NONE, true),   // pass all (default)
  AppFeature("1 Block Clock", 1, FEATURE_GROUP_FILTER, BLOCK_CLOCK),
  AppFeature("1 Clock Only", 1, FEATURE_GROUP_FILTER, FILTER_CLOCK),        // only clock/start/stop

  AppFeature("2 Block None", 2, FEATURE_GROUP_FILTER, BLOCK_NONE, true),
  AppFeature("2 Block Clock", 2, FEATURE_GROUP_FILTER, BLOCK_CLOCK),
  AppFeature("2 Clock Only", 2, FEATURE_GROUP_FILTER, FILTER_CLOCK),

  AppFeature("3 Block None", 3, FEATURE_GROUP_FILTER, BLOCK_NONE, true),
  AppFeature("3 Block Clock", 3, FEATURE_GROUP_FILTER, BLOCK_CLOCK),
  AppFeature("3 Clock Only", 3, FEATURE_GROUP_FILTER, FILTER_CLOCK),


  
  AppFeature(" ", 0, FEATURE_GROUP_PLACEHOLDER, 0)
};

const uint8_t FEATURECOUNT = 203;
int iMenuPosition = -3;
uint8_t iRootNoteOffset = 0;

// Per-output settings: index 0 = MIDI out 1, 1 = MIDI out 2, 2 = USB out
typedef struct {
  uint8_t velocity;
  uint8_t noteChannel;
  uint8_t ccChannel;
  uint8_t scale;
  uint8_t rootNote;
  uint8_t clockFilter;  // BLOCK_NONE, BLOCK_CLOCK, FILTER_CLOCK (FEATURE_GROUP_FILTER)
} OutputSettings;

// Resolved settings from arrFeatures[] – synced on encoder click, used without for-loops
typedef struct {
  uint8_t routingIn1;
  uint8_t routingIn2;
  uint8_t routingIn3;
  OutputSettings output[3];
} AppSettings;

static const OutputSettings kDefaultOutput = {
  VELOCITY_PASSTHRU, CHANNEL_PASSTHRU, CHANNEL_PASSTHRU,
  SCALE_PASSTHRU, ROOTNOTE_PASSTHROUGH, BLOCK_NONE
};

AppSettings settings = {
  ROUTING_TO_NONE, ROUTING_TO_NONE, ROUTING_TO_NONE,
  { kDefaultOutput, kDefaultOutput, kDefaultOutput }
};

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

  displayText("esp32 Midi", "processor", "Firmware:", VERSION);
  delay(2000);
  EEPROM.begin(EEPROM_SIZE);
  processMenuNavigation(0);
  syncSettingsFromFeatures();

  loadPreset(0);
}//Setup

// Save current settings to preset slot (0=A, 1=B, 2=C, 3=D). Persists across power cycle.
void savePreset(uint8_t slot) {
  if (slot >= PRESET_COUNT) return;
  uint16_t addr = 2 + slot * (1 + sizeof(AppSettings));
  EEPROM.write(addr, 1);  // valid
  EEPROM.put(addr + 1, settings);
  EEPROM.commit();
  tmrDisplay.RESET;
  displayText("Preset", String((char)('A' + slot)) + " saved", "", "");
}

// Load preset into current settings and update menu. No-op if slot was never saved.
void loadPreset(uint8_t slot) {
  if (slot >= PRESET_COUNT) return;
  uint16_t addr = 2 + slot * (1 + sizeof(AppSettings));
  if (EEPROM.read(addr) != 1) {
    //displayText("Preset", String((char)('A' + slot)) + " empty", "", "");
    displayText("Preset " + String((char)('A' + slot)),  "", "empty", "");
    return;
  }
  EEPROM.get(addr + 1, settings);
  for (uint8_t o = 0; o < 3; o++) {
    if (settings.output[o].clockFilter != BLOCK_NONE && settings.output[o].clockFilter != BLOCK_CLOCK && settings.output[o].clockFilter != FILTER_CLOCK)
      settings.output[o].clockFilter = BLOCK_NONE;
  }
  syncFeaturesFromSettings();
  iRootNoteOffset = (settings.output[0].rootNote > ROOTNOTE_PASSTHROUGH) ? (settings.output[0].rootNote - 1) : 0;
  //tmrDisplay.RESET;
  //displayText("Preset", String((char)('A' + slot)) + " loaded", "", "");
  //displayText("Preset " + String((char)('A' + slot)), "",  "loaded", "");
  displayPreset( String((char)('A' + slot)) );
}

// Update arrFeatures[] selection to match current settings (after load preset).
void syncFeaturesFromSettings() {
  for (uint8_t i = 0; i < FEATURECOUNT; i++) {
    arrFeatures[i].select(false);
  }
  for (uint8_t i = 0; i < FEATURECOUNT; i++) {
    uint8_t grp = arrFeatures[i].getFeatureGroup();
    uint8_t val = arrFeatures[i].getFeature();
    uint8_t outport = arrFeatures[i].getOutport();
    uint8_t o = (outport >= 1 && outport <= 3) ? (outport - 1) : 0;
    bool match = false;
    if (grp == FEATURE_GROUP_ROUTING_IN_1 && outport == 0 && val == settings.routingIn1) match = true;
    else if (grp == FEATURE_GROUP_ROUTING_IN_2 && outport == 0 && val == settings.routingIn2) match = true;
    else if (grp == FEATURE_GROUP_ROUTING_IN_3 && outport == 0 && val == settings.routingIn3) match = true;
    else if (grp == FEATURE_GROUP_VELOCITY && outport >= 1 && settings.output[o].velocity == val) match = true;
    else if (grp == FEATURE_GROUP_NOTE_CHANNEL && outport >= 1 && settings.output[o].noteChannel == val) match = true;
    else if (grp == FEATURE_GROUP_CC_CHANNEL && outport >= 1 && settings.output[o].ccChannel == val) match = true;
    else if (grp == FEATURE_GROUP_SCALE && outport >= 1 && settings.output[o].scale == val) match = true;
    else if (grp == FEATURE_GROUP_ROOTNOTE && outport >= 1 && settings.output[o].rootNote == val) match = true;
    else if (grp == FEATURE_GROUP_FILTER && outport >= 1 && settings.output[o].clockFilter == val) match = true;
    if (match) arrFeatures[i].select(true);
  }
}

// Build settings from current arrFeatures[] selection (call after processEncoderClick / at startup)
// Only updates the specific output for each selected feature; other outputs keep their values.
void syncSettingsFromFeatures() {
  settings.routingIn1 = ROUTING_TO_NONE;
  settings.routingIn2 = ROUTING_TO_NONE;
  settings.routingIn3 = ROUTING_TO_NONE;

  for (uint8_t i = 0; i < FEATURECOUNT; i++) {
    if (!arrFeatures[i].isSelected()) continue;
    uint8_t grp = arrFeatures[i].getFeatureGroup();
    uint8_t val = arrFeatures[i].getFeature();
    uint8_t outport = arrFeatures[i].getOutport();
    uint8_t o = (outport >= 1 && outport <= 3) ? (outport - 1) : 0;

    if (grp == FEATURE_GROUP_ROUTING_IN_1) settings.routingIn1 = val;
    else if (grp == FEATURE_GROUP_ROUTING_IN_2) settings.routingIn2 = val;
    else if (grp == FEATURE_GROUP_ROUTING_IN_3) settings.routingIn3 = val;
    else if (grp == FEATURE_GROUP_VELOCITY) settings.output[o].velocity = val;
    else if (grp == FEATURE_GROUP_NOTE_CHANNEL) settings.output[o].noteChannel = val;
    else if (grp == FEATURE_GROUP_CC_CHANNEL) settings.output[o].ccChannel = val;
    else if (grp == FEATURE_GROUP_SCALE) settings.output[o].scale = val;
    else if (grp == FEATURE_GROUP_ROOTNOTE) {
      settings.output[o].rootNote = val;
      if (o == 0) iRootNoteOffset = (val > ROOTNOTE_PASSTHROUGH) ? (val - 1) : 0;
    }
    else if (grp == FEATURE_GROUP_FILTER) settings.output[o].clockFilter = val;
  }
}

void loop() {
  Usb.Task();
  if ( Midi ) {
    uint8_t size;
    do {
      size = Midi.RecvData(midiPacket_IN3.data);
      if (size > 0) {
        midiPacket_IN3.drop = false;
        pushMidiQueue(&midiPacket_IN3, 3);  // USB = source 3; passed through based on routing
        pixels.setPixelColor(4, pixels.Color(LED_ON, LED_ON, LED_OFF));
        pixels.show();
        tmrUSB.RESET;
      }
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

  // Re-show display after loading or storing a preset
  if(tmrDisplay.FIRST_TRIGGER){
    displayText(getMenuItem(iMenuPosition), getMenuItem(iMenuPosition + 1), getMenuItem(iMenuPosition + 2), "");
  }


}// Loop

/*
  Drain serial/USB into the MIDI queue, then process queued events in order (FIFO, up to 10).
  Every MIDI message (notes, CC, program change, pitch bend, realtime, etc.) is passed through
  solely according to routing settings; no message type is blocked except by routing.
*/
void readData(){
  checkMidiIn_1();
  checkMidiIn_2();
  checkMidiIn_USB();
  processMidiQueue();
}

// Process all queued MIDI events in order (FIFO). Each event is forwarded based on routing only.
void processMidiQueue() {
  QueuedMidiEvent ev;
  while (popMidiQueue(&ev)) {
    sendPacket(ev.source, &ev.pkt);
  }
}

void copyData(uint8_t *theArray, uint8_t pInPacket[]) {
  for (uint8_t i = 0; i < 3; i++) {
    theArray[i] = pInPacket[i];
  }
}

// True if this routing target sends to the given output (0=out1, 1=out2, 2=USB)
bool routingSendsToOutput(uint8_t routing, uint8_t outIndex) {
  if (routing == ROUTING_TO_NONE) return false;
  switch (routing) {
    case ROUTING_TO_1:  return outIndex == 0;
    case ROUTING_TO_2:  return outIndex == 1;
    case ROUTING_TO_3:  return outIndex == 2;
    case ROUTING_TO_12: return outIndex == 0 || outIndex == 1;
    case ROUTING_TO_13: return outIndex == 0 || outIndex == 2;
    case ROUTING_TO_23: return outIndex == 1 || outIndex == 2;
    case ROUTING_TO_123: return true;
    default: return false;
  }
}

// Return MIDI packet length by status (2 for program change/channel pressure, 3 for most, 1 for realtime).
static uint8_t getMidiPacketLen(uint8_t status) {
  uint8_t s = status & 0xF0;
  if (s == 0xC0 || s == 0xD0) return 2;  // program change, channel pressure
  if (s >= 0xF0) return 1;               // system / realtime
  return 3;                               // note on/off, aftertouch, CC, pitch bend
}

// Send packet to a single output (0=Serial1, 1=Serial2, 2=USB). Skip if p->drop.
void sendToOutput(uint8_t outIndex, MidiPacket *p) {
  if (p->drop) return;
  uint8_t status = p->data[0] & 0xF0;
  uint8_t len = getMidiPacketLen(status);
  flashLED(outIndex + 1);
  if (outIndex == 0) Serial1.write(p->data, len);
  else if (outIndex == 1) Serial2.write(p->data, len);
  else if (outIndex == 2) Midi.SendData(p->data, 0);
}

// FEATURE_GROUP_FILTER: BLOCK_NONE=pass all, BLOCK_CLOCK=drop clock/start/stop, FILTER_CLOCK=only pass clock/start/stop
void processClockFilter(MidiPacket *p, uint8_t outIndex) {
  uint8_t status = p->data[0];
  bool isRealtime = (status == MIDI_CLOCK) || (status == MIDI_START) || (status == MIDI_STOP);
  uint8_t f = settings.output[outIndex].clockFilter;
  if (f == BLOCK_NONE) return;
  if (f == BLOCK_CLOCK && isRealtime) { p->drop = true; return; }
  if (f == FILTER_CLOCK && !isRealtime) { p->drop = true; return; }
}

// Only pass through notes that fit the selected scale and root. Others: set p->drop = true.
// Implemented: SCALE_MAJOR and SCALE_MINOR (natural minor) for every root note (C through B).
void processScale(MidiPacket *p, uint8_t outIndex) {
  uint8_t status = p->data[0] & 0xF0;
  if (status != 0x80 && status != 0x90) return; // only filter note on/off

  uint8_t scale = settings.output[outIndex].scale;
  uint8_t root = settings.output[outIndex].rootNote;
  if (scale == SCALE_PASSTHRU || root == ROOTNOTE_PASSTHROUGH) return;
  if (root < ROOTNOTE_C || root > ROOTNOTE_H) return; // invalid root

  // Root note enum 1..12 (C..B) -> pitch class 0..11
  uint8_t rootPc = root - 1;

  // Intervals in semitones from root: major 0,2,4,5,7,9,11 ; natural minor 0,2,3,5,7,8,10 ; pentatonic 5 notes
  static const uint8_t majorIntervals[]   = { 0, 2, 4, 5, 7, 9, 11 };
  static const uint8_t minorIntervals[]   = { 0, 2, 3, 5, 7, 8, 10 };
  static const uint8_t pentaMajorIntervals[] = { 0, 2, 4, 7, 9 };
  static const uint8_t pentaMinorIntervals[] = { 0, 3, 5, 7, 10 };

  const uint8_t *intervals;
  uint8_t nNotes = 7;
  if (scale == SCALE_MAJOR) {
    intervals = majorIntervals;
  } else if (scale == SCALE_MINOR) {
    intervals = minorIntervals;
  } else if (scale == SCALE_PENTATONIC_MAJOR) {
    intervals = pentaMajorIntervals;
    nNotes = 5;
  } else if (scale == SCALE_PENTATONIC_MINOR) {
    intervals = pentaMinorIntervals;
    nNotes = 5;
  } else {
    return;
  }

  uint8_t pc = p->data[1] % 12;
  bool inScale = false;
  for (uint8_t i = 0; i < nNotes; i++) {
    if ((rootPc + intervals[i]) % 12 == pc) { inScale = true; break; }
  }
  if (!inScale) p->drop = true;
}

void processVelocity(uint8_t *midiPacket, uint8_t outIndex){
  uint8_t iStatus = midiPacket[0] & 0xF0;
  if ((iStatus != 0x80) && (iStatus != 0x90)) return; // Nur midi noten
  // Note OFF (0x80): always send velocity 0 so it stays a note off
  if ((iStatus == 0x80)||(midiPacket[2]==0)) {
    midiPacket[2] = 0;
    return;
  }
  // Note ON (0x90): apply velocity setting
  uint8_t v = settings.output[outIndex].velocity;
  switch (v) {
    case VELOCITY_FIX_63:   midiPacket[2] = 63; break;
    case VELOCITY_FIX_100:  midiPacket[2] = 100; break;
    case VELOCITY_FIX_127:  midiPacket[2] = 127; break;
    case VELOCITY_RANDOM:   midiPacket[2] = random(128); break;
    case VELOCITY_RANDOM_100: midiPacket[2] = 91 + random(20); break;
    default: break; // VELOCITY_PASSTHRU
  }
}

void process_Note_Channel(uint8_t *midiPacket, uint8_t outIndex){
  uint8_t iStatus = midiPacket[0] & 0xF0;
  if ((iStatus != 0x80) && (iStatus != 0x90)) return; // Nur midi noten
  if (settings.output[outIndex].noteChannel != CHANNEL_PASSTHRU) {
    midiPacket[0] = (midiPacket[0] & 0xF0) + settings.output[outIndex].noteChannel - 1;
  }
}

void process_CC_Channel(uint8_t *midiPacket, uint8_t outIndex){
  if ((midiPacket[0] & 0xF0) != 0xB0) return; // Nur midi CCs
  if (settings.output[outIndex].ccChannel != CHANNEL_PASSTHRU) {
    midiPacket[0] = (midiPacket[0] & 0xF0) + settings.output[outIndex].ccChannel - 1;
  }
}

// Forward one MIDI packet to outputs based only on routing. All message types (notes, CC,
// program change, pitch bend, realtime, etc.) are passed through; routing is the only gate.
void sendPacket(uint8_t pInFrom, MidiPacket *pkt) {
  if (pkt->drop) return;

  uint8_t iRoutingTarget;
  if (pInFrom == 1) iRoutingTarget = settings.routingIn1;
  else if (pInFrom == 2) iRoutingTarget = settings.routingIn2;
  else iRoutingTarget = settings.routingIn3;

  // Routing is the sole decider: no routing for this input => no output
  if (iRoutingTarget == ROUTING_TO_NONE) return;

  // Send to each destination selected by routing; per-output modifiers only affect notes/CC.
  MidiPacket tmp;
  for (uint8_t outIndex = 0; outIndex < 3; outIndex++) {
    if (!routingSendsToOutput(iRoutingTarget, outIndex)) continue;
    copyData(tmp.data, pkt->data);
    tmp.drop = false;
    processVelocity(tmp.data, outIndex);   // only touches note on/off
    process_Note_Channel(tmp.data, outIndex);
    process_CC_Channel(tmp.data, outIndex);
    processScale(&tmp, outIndex);           // only can set drop for out-of-scale notes
    processClockFilter(&tmp, outIndex);     // block or pass MIDI clock (and start/stop) per output
    sendToOutput(outIndex, &tmp);
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
  if (btnA.isHeld()) {
    if (!bBtnA_old) {
      bBtnA_old = true;
      btnA_Held = true;
      bBtnA_Reset = btnA.resetClicked();
      savePreset(0);  // long press: save to preset A
    }
  } else {
    bBtnA_old = false;
    if (btnA.isClicked()) {
      if (bBtnA_Reset == false && btnA_Held == false) loadPreset(0);  // short click: recall preset A
      btnA.resetClicked();
      bBtnA_Reset = false;
      btnA_Held = false;
    }
  }
}

void checkButton_B(){
   btnB.handle();
  if (btnB.isHeld()) {
    if (!bBtnB_old) {
      bBtnB_old = true;
      btnB_Held = true;
      bBtnB_Reset = btnB.resetClicked();
      savePreset(1);  // long press: save to preset B
    }
  } else {
    bBtnB_old = false;
    if (btnB.isClicked()) {
      if (bBtnB_Reset == false && btnB_Held == false) loadPreset(1);  // short click: recall preset B
      btnB.resetClicked();
      bBtnB_Reset = false;
      btnB_Held = false;
    }
  }
}

void checkButton_C(){
  btnC.handle();
  if (btnC.isHeld()) {
    if (!bBtnC_old) {
      bBtnC_old = true;
      btnC_Held = true;
      bBtnC_Reset = btnC.resetClicked();
      savePreset(2);  // long press: save to preset C
    }
  } else {
    bBtnC_old = false;
    if (btnC.isClicked()) {
      if (bBtnC_Reset == false && btnC_Held == false) loadPreset(2);  // short click: recall preset C
      btnC.resetClicked();
      bBtnC_Reset = false;
      btnC_Held = false;
    }
  }
}

void checkButton_D(){
  btnD.handle();
  if (btnD.isHeld()) {
    if (!bBtnD_old) {
      bBtnD_old = true;
      btnD_Held = true;
      bBtnD_Reset = btnD.resetClicked();
      savePreset(3);  // long press: save to preset D
    }
  } else {
    bBtnD_old = false;
    if (btnD.isClicked()) {
      if (bBtnD_Reset == false && btnD_Held == false) loadPreset(3);  // short click: recall preset D
      btnD.resetClicked();
      bBtnD_Reset = false;
      btnD_Held = false;
    }
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

// Read up to 3 bytes from Serial1; when complete, push to queue (source 1). Realtime (clock/start/stop) = 1 byte, push immediately.
void checkMidiIn_1(){
  while ((Serial1.available() > 0) && (iCounter_IN1 < 3)) {
    pixels.setPixelColor(0, pixels.Color(LED_ON, LED_ON, LED_OFF));
    pixels.show();
    tmrIn1.RESET;
    byte tmp = Serial1.read();
    midiPacket_IN1.data[iCounter_IN1] = tmp;
    if ((tmp == MIDI_START) || (tmp == MIDI_STOP) || (tmp == MIDI_CLOCK)) {
      midiPacket_IN1.drop = false;
      pushMidiQueue(&midiPacket_IN1, 1);
      iCounter_IN1 = 0;
      continue;
    }
    iCounter_IN1++;
    if (iCounter_IN1 == 3) {
      iCounter_IN1 = 0;
      pushMidiQueue(&midiPacket_IN1, 1);
    }
  }
}

// Read up to 3 bytes from Serial2; when complete, push to queue (source 2). Realtime (clock/start/stop) = 1 byte, push immediately.
void checkMidiIn_2(){
  while ((Serial2.available() > 0) && (iCounter_IN2 < 3)) {
    pixels.setPixelColor(1, pixels.Color(LED_ON, LED_ON, LED_OFF));
    pixels.show();
    tmrIn2.RESET;
    byte tmp = Serial2.read();
    midiPacket_IN2.data[iCounter_IN2] = tmp;
    if ((tmp == MIDI_START) || (tmp == MIDI_STOP) || (tmp == MIDI_CLOCK)) {
      midiPacket_IN2.drop = false;
      pushMidiQueue(&midiPacket_IN2, 2);
      iCounter_IN2 = 0;
      continue;
    }
    iCounter_IN2++;
    if (iCounter_IN2 == 3) {
      iCounter_IN2 = 0;
      pushMidiQueue(&midiPacket_IN2, 2);
    }
  }
}

// USB packets are pushed in loop() when RecvData returns; here we only flash LED if queue had USB data
void checkMidiIn_USB(){
  (void)0;  // LED for USB activity could be driven by queue if we tracked last source
}

 
void onInit()
{
  char buf[20];
  uint16_t vid = Midi.idVendor();
  uint16_t pid = Midi.idProduct();
  sprintf(buf, "VID:%04X, PID:%04X", vid, pid);
  Serial.println(buf); 
}

void displayPreset(String pPreset){
  display.clearDisplay();
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 12);
  display.println( ("Preset") );
  display.setFont(&FreeSans24pt7b);
  display.setTextSize(1);
  display.setCursor(45, 60);
  display.println( (pPreset) );
  display.display();
  display.setFont(&FreeSans9pt7b);
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
  // Deselect only others in the same feature group AND same output (so each output keeps its own choice)
  uint8_t tmpFG = arrFeatures[iMenuPosition].getFeatureGroup();
  uint8_t tmpOutport = arrFeatures[iMenuPosition].getOutport();
  for (uint8_t i = 0; i < FEATURECOUNT; i++) {
    if (arrFeatures[i].getFeatureGroup() == tmpFG && arrFeatures[i].getOutport() == tmpOutport) {
      arrFeatures[i].select(false);
    }
  }

  // If scale passthrough is selected for this output, select root-note-passthrough for same output only.
  if (arrFeatures[iMenuPosition].getFeatureGroup() == FEATURE_GROUP_SCALE) {
    if (arrFeatures[iMenuPosition].getFeature() == SCALE_PASSTHRU) {
      for (uint8_t i = 0; i < FEATURECOUNT; i++) {
        if (arrFeatures[i].getFeatureGroup() == FEATURE_GROUP_ROOTNOTE && arrFeatures[i].getOutport() == tmpOutport) {
          if (arrFeatures[i].getFeature() == ROOTNOTE_PASSTHROUGH) {
            arrFeatures[i].select(true);
          } else {
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

  syncSettingsFromFeatures();
  displayText(getMenuItem( iMenuPosition ), getMenuItem( iMenuPosition+1 ), getMenuItem( iMenuPosition+2 ) , "");
}
