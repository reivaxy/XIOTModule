/**
 *  Message with retries
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */


#pragma once
#include <Arduino.h>
#include <XUtils.h>

enum MessageType {MESSAGE_MODULE, MESSAGE_PING, MESSAGE_LOG, MESSAGE_ALERT, MESSAGE_CUSTOM, RECORD_CUSTOM};


class Message {
public:
   Message(MessageType type, const char* message);
   Message(const char* customType, const char* id, const char* message);
   ~Message();

   MessageType type;
   char* message;
   char* customType;
   char* id;  // events have an automatically generated id, records have a provided id
   int retryCount = 0;
   unsigned long initialTime = 0; // to compute sending delay
};

