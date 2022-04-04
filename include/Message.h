/**
 *  Message with retries
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */


#pragma once
#include <Arduino.h>

enum MessageType {MESSAGE_MODULE, MESSAGE_PING, MESSAGE_LOG, MESSAGE_ALERT, MESSAGE_CUSTOM, RECORD_CUSTOM};


class Message {
public:
   Message(MessageType type, String message);
   Message(const char* customType, String id, String message);

   MessageType type;
   String message;
   String customType;
   String id;  // events have an automatically generated id, records have a provided id
   int retryCount = 0;
   unsigned long initialTime = 0; // to compute sending delay
};

