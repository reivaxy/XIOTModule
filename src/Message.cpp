/**
 *  Message with retries
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include  "Message.h"

Message::Message(MessageType type, String message) {
   this->type = type;
   this->message = message;
}

Message::Message(const char* customType, String message) {
   this->type = MESSAGE_CUSTOM;
   strncpy(this->customType, customType, CUSTOM_TYPE_LENGTH_MAX);
   this->customType[CUSTOM_TYPE_LENGTH_MAX] = 0;
   this->message = message;
}