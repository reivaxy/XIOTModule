/**
 *  Firebase recording class
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#pragma once
#include "XIOTModuleDebug.h"

#include <Arduino.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <ArduinoJson.h>
#include <XIOTConfig.h>
#include "XIOTMessages.h"

// TODO handle authent
class Firebase {
public:
  Firebase(ModuleConfigClass* config);

  void loop();
  int sendRecord(const char* type, JsonObject* jsonBuffer); 
  int sendEvent(const char* type, JsonObject* jsonBuffer); 
  int sendLog(const char* message); 
  int sendAlert(const char* message); 
  int sendLog(const char* message, const char* type); 
  void reset();
  void init();
  char* getDateStr(char* dateBuffer);
  int sendToFirebase(const char* method, const char* url, JsonObject* jsonBufferRoot);
  int sendToFirebase(const char* method, const char* url, const char* payload);
  void setCommonFields(JsonObject *jsonBufferRoot);


  ModuleConfigClass* config;
  unsigned long lastSendPing = 0;
  char macAddrStr[20];
  boolean initialized = false;

};


