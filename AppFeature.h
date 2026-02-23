// MenuItem.h
#ifndef AppFeature_h
#define AppFeature_h

#include <Arduino.h>

const uint8_t FEATURE_GROUP_PLACEHOLDER = 0;
const uint8_t FEATURE_GROUP_VELOCITY = 1;
const uint8_t FEATURE_GROUP_SCALE = 2;
const uint8_t FEATURE_GROUP_ROOTNOTE = 4;
const uint8_t FEATURE_GROUP_NOTE_CHANNEL = 8;
const uint8_t FEATURE_GROUP_CC_CHANNEL = 16;
const uint8_t FEATURE_GROUP_SCALE_HANDLER = 32;
const uint8_t FEATURE_GROUP_FILTER = 65;   // distinct from routing (32,64,128): clock block/pass per output
//
const uint8_t FEATURE_GROUP_ROUTING_IN_1 = 32;
const uint8_t FEATURE_GROUP_ROUTING_IN_2 = 64;
const uint8_t FEATURE_GROUP_ROUTING_IN_3 = 128;

const uint8_t ROUTING_TO_NONE = 0;
const uint8_t ROUTING_TO_1 = 1;
const uint8_t ROUTING_TO_2 = 2;
const uint8_t ROUTING_TO_3 = 3;
const uint8_t ROUTING_TO_12 = 4;
const uint8_t ROUTING_TO_13 = 5;
const uint8_t ROUTING_TO_23 = 6;
const uint8_t ROUTING_TO_123 = 7;
/*
const uint8_t ROUTING_2_TO_NONE = 0;
const uint8_t ROUTING_2_TO_1 = 1;
const uint8_t ROUTING_2_TO_2 = 2;
const uint8_t ROUTING_2_TO_3 = 3;
const uint8_t ROUTING_2_TO_12 = 4;
const uint8_t ROUTING_2_TO_13 = 5;
const uint8_t ROUTING_2_TO_23 = 6;
const uint8_t ROUTING_2_TO_123 = 7;

const uint8_t ROUTING_3_TO_NONE = 0;
const uint8_t ROUTING_3_TO_1 = 1;
const uint8_t ROUTING_3_TO_2 = 2;
const uint8_t ROUTING_3_TO_3 = 4;
const uint8_t ROUTING_3_TO_12 = 3;
const uint8_t ROUTING_3_TO_13 = 5;
const uint8_t ROUTING_3_TO_23 = 6;
const uint8_t ROUTING_3_TO_123 = 7;
*/

const uint8_t VELOCITY_PASSTHRU = 0;
const uint8_t VELOCITY_FIX_63 = 1;
const uint8_t VELOCITY_FIX_100 = 2;
const uint8_t VELOCITY_RANDOM_100 = 3;
const uint8_t VELOCITY_FIX_127 = 4;
const uint8_t VELOCITY_RANDOM = 5;

const uint8_t SCALE_PASSTHRU = 0;
const uint8_t SCALE_MAJOR = 1;
const uint8_t SCALE_MINOR = 2;
const uint8_t SCALE_PENTATONIC_MAJOR = 3;
const uint8_t SCALE_PENTATONIC_MINOR = 4;

const uint8_t SCALE_HANDLER_FILTER = 0;
const uint8_t SCALE_HANDLER_MAPPER = 1;

const uint8_t ROOTNOTE_PASSTHROUGH = 0;
const uint8_t ROOTNOTE_C = 01;
const uint8_t ROOTNOTE_Cs = 2;
const uint8_t ROOTNOTE_D = 3;
const uint8_t ROOTNOTE_Ds = 4;
const uint8_t ROOTNOTE_E = 5;
const uint8_t ROOTNOTE_F = 6;
const uint8_t ROOTNOTE_Fs = 7;
const uint8_t ROOTNOTE_G = 8;
const uint8_t ROOTNOTE_Gs = 9;
const uint8_t ROOTNOTE_A = 10;
const uint8_t ROOTNOTE_As = 11;
const uint8_t ROOTNOTE_H = 12;


const uint8_t CHANNEL_PASSTHRU = 0;
const uint8_t CHANNEL_1 = 1;
const uint8_t CHANNEL_2 = 2;
const uint8_t CHANNEL_3 = 3;
const uint8_t CHANNEL_4 = 4;
const uint8_t CHANNEL_5 = 5;
const uint8_t CHANNEL_6 = 6;
const uint8_t CHANNEL_7 = 7;
const uint8_t CHANNEL_8 = 8;
const uint8_t CHANNEL_9 = 9;
const uint8_t CHANNEL_10 = 10;
const uint8_t CHANNEL_11 = 11;
const uint8_t CHANNEL_12 = 12;
const uint8_t CHANNEL_13 = 13;
const uint8_t CHANNEL_14 = 14;
const uint8_t CHANNEL_15 = 15;
const uint8_t CHANNEL_16 = 16;

const uint8_t BLOCK_NONE = 1;
const uint8_t BLOCK_CLOCK = 2;  
const uint8_t FILTER_CLOCK = 3; // Only let CLOCK, START and STOP pass through

class AppFeature {

  private:
    uint8_t iFeature = -1;
    //uint8_t iOutport = -1;
    uint8_t iFeatureGroup = -1;
    bool bSelected = false;
    String sText;
    uint8_t iOutport;
    

  public:
    AppFeature(String pText, uint8_t pOutport, uint8_t pFeatureGroup, uint8_t pFeature);
    AppFeature(String pText, uint8_t pOutport, uint8_t pFeatureGroup, uint8_t pFeature, bool pSelect);
    uint8_t getFeatureGroup();
    uint8_t getFeature();
    String getText();
    void select(bool p);
    bool isSelected();
    uint8_t getOutport();

};

#endif