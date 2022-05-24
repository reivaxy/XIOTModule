/**
 *  Firebase recording class
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#pragma once
#include "XIOTModuleDebug.h"

#include <Arduino.h>
#include <TimeLib.h>
#include <pgmspace.h>
#include <ArduinoJson.h>
#include <XIOTConfig.h>
#include "XIOTMessages.h"
#include "XIOTModuleDebug.h"
#include "XIOTHttps.h"
#include "Message.h"

#define MAX_DIFFERED_MESSAGES_COUNT 20
#define HANDLE_DIFFERED_MESSAGES_DEFAULT_DELAY_MS 1000
#define MAX_RETRY_MESSAGE_COUNT 10
#define MAX_RETRY_DELAY 5000
#define DATE_BUFFER_SIZE 25

// TODO handle authent
class Firebase {
public:
  Firebase(ModuleConfigClass* config);

  void loop();
  void disable();
  void init(const char* macAddrStr);

  void setCommonFields(JsonObject *jsonBufferRoot);
  char* getDateStr();
  int sendEvent(const char* type, const char* message); 
  int sendToFirebase(const char* method, const char* url, JsonObject* jsonBufferRoot);
  int sendToFirebase(const char* method, const char* url, const char* payload);

  void differLog(const char* message);
  void differLog(JsonObject* jsonBufferRoot);
  void differAlert(const char* message);

  void differMessage(MessageType type, const char* message);
  void differMessage(MessageType type,  JsonObject* jsonBufferRoot);
  
  void differRecord(MessageType type, JsonObject* jsonBuffer); 
  void handleDifferedMessages();
  size_t getBufferSize(int fieldCount);
  ModuleConfigClass* config;
  unsigned long lastSendPing = 0;
  char macAddrStr[20];
  boolean initialized = false;

  Message* differedMessages[MAX_DIFFERED_MESSAGES_COUNT];
  unsigned long lastHandledDifferedMessage = 0;
  unsigned long currentDifferedMessageDelay = HANDLE_DIFFERED_MESSAGES_DEFAULT_DELAY_MS;
  unsigned int lostMessageCount = 0;
  unsigned int failedMessageCount = 0;
  unsigned int retriedMessageCount = 0;

  bool sendInitPing = true;
  char date[DATE_BUFFER_SIZE]; // date field in json payloads

};


