// MenuItem.cpp
#include "AppFeature.h"



AppFeature::AppFeature(String pText, uint8_t pOutport, uint8_t pFeatureGroup, uint8_t pFeature){
  iFeatureGroup = pFeatureGroup;
  sText = pText;
  iFeature = pFeature;
  iOutport = pOutport;
}

AppFeature::AppFeature(String pText, uint8_t pOutport, uint8_t pFeatureGroup, uint8_t pFeature, bool pSelect){
  iFeatureGroup = pFeatureGroup;
  sText = pText;
  iFeature = pFeature;
  iOutport = pOutport;
  bSelected = true;
}

String AppFeature::getText(){
  return sText;
}

uint8_t AppFeature::getFeatureGroup(){
  return iFeatureGroup;
}

uint8_t AppFeature::getFeature(){
  return iFeature;
}


void AppFeature::select(bool p){
  bSelected = p;
}

bool AppFeature::isSelected(){
  return bSelected;
}

uint8_t AppFeature::getOutport(){
  return iOutport;
}