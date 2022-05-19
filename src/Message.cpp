/**
 *  Message with retries
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include  "Message.h"

Message::Message(MessageType type, const char* message) {
   this->type = type;
   this->message = XUtils::mallocAndCopy(message);
   this->id = NULL;
   this->customType = NULL;

}

Message::Message(const char* customType, const char* id, const char* message) {
   this->type = id == NULL ? MESSAGE_CUSTOM : RECORD_CUSTOM ; // is this an event or a record. events have self generated ids, records don't
   this->id = XUtils::mallocAndCopy(id);
   this->customType = XUtils::mallocAndCopy(customType);
   this->message = XUtils::mallocAndCopy(message);
}

Message::~Message() {
   free(this->id);
   free(this->customType);
   free(this->message);

}