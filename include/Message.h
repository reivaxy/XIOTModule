/**
 *  Message with retries
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */


#pragma once
#include <Arduino.h>

enum MessageType {MESSAGE_MODULE, MESSAGE_PING, MESSAGE_LOG, MESSAGE_ALERT, MESSAGE_CUSTOM, RECORD_CUSTOM};

#define CUSTOM_TYPE_LENGTH_MAX 10

class Message {
public:
   Message(MessageType type, String message);
   Message(const char* customType, String message);

   MessageType type;
   String message;
   char customType[CUSTOM_TYPE_LENGTH_MAX + 1];  // use dynamic allocation instead ?
   int retryCount = 0;
   unsigned long initialTime = 0; // to compute sending delay
};

