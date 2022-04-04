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

Message::Message(const char* customType, String id, String message) {
   this->type = id == NULL ? MESSAGE_CUSTOM : RECORD_CUSTOM ; // is this an event or a record. events have self generated ids, records don't
   this->id = id;
   this->customType = customType;
   this->message = message;
}