/*
   @title     StarMod
   @file      Module.h
   @date      20230807
   @repo      https://github.com/ewowi/StarMod
   @Authors   https://github.com/ewowi/StarMod/commits/main
   @Copyright (c) 2023 Github StarMod Commit Authors
   @license   GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
*/

#pragma once
#include <Arduino.h>

#include "ArduinoJson.h"

class Module {

public:
  const char * name;
  bool success;
  bool isEnabled;
  unsigned long secondMillis = 0; // Feels like it should be private, bit doesn't compile if set as such

  JsonObject parentVar;

  Module(const char * name) {
    this->name = name;
    success = true;
    isEnabled = true;
    // Serial.printf("Constructor %s %s\n", __PRETTY_FUNCTION__, name);
  }

  virtual void setup() {}

  virtual void loop() {}

  virtual void connectedChanged() {}
  virtual void enabledChanged() {}

  virtual void testManager() {}
  virtual void performanceManager() {}
  virtual void dataSizeManager() {}
  virtual void codeSizeManager() {}
};