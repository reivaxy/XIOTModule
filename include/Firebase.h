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
#include "XIOTModuleDebug.h"

#define MAX_DIFFERED_MESSAGES_COUNT 10
#define HANDLE_DIFFERED_MESSAGES_DELAY 2000  // 2s

// TODO handle authent
class Firebase {
public:
  Firebase(ModuleConfigClass* config);

  void loop();
  int sendRecord(const char* type, JsonObject* jsonBuffer); 
  int sendEvent(const char* type, JsonObject* jsonBuffer); 
  int sendLog(const char* message); 
  int sendAlert(const char* message); 
  int sendLog(const char* type, const char* message); 
  void reset();
  void init(const char* macAddrStr);
  char* getDateStr(char* dateBuffer);
  int sendToFirebase(const char* method, const char* url, JsonObject* jsonBufferRoot);
  int sendToFirebase(const char* method, const char* url, char* payload);
  void setCommonFields(JsonObject *jsonBufferRoot);
  void sendDifferedLog(const char* logMessage);
  void handleDifferedLogs();

  ModuleConfigClass* config;
  unsigned long lastSendPing = 0;
  char macAddrStr[20];
  boolean initialized = false;
  String *differedMessages[MAX_DIFFERED_MESSAGES_COUNT];
  unsigned long lastHandledDifferedMessage = 0;
  
  bool sendInitPing = true;


};


