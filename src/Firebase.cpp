/**
 *  Firebase recording class
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include "Firebase.h"
#include <exception>


#define COMMON_FIELD_COUNT 5
#define PING_PERIOD 1000 * 60 * 5UL // 5mn
#define DATE_BUFFER_SIZE 25

#define URL_MAX_LENGTH_WITHOUT_SECRET 100

Firebase::Firebase(ModuleConfigClass* config) {
  this->config = config;
  for (int i=0; i < MAX_DIFFERED_MESSAGES_COUNT; i++) {
    differedMessages[i] = NULL;
  }
}

   
void Firebase::init(const char* macAddrStr) {
  Debug("Firebase::init\n");
  strcpy(this->macAddrStr, macAddrStr);
  Debug("Heap at begining of Firebase::init: %d\n", system_get_free_heap_size()); 
  lastSendPing = 0;
  if (strlen(config->getFirebaseUrl()) > 10) {
    initialized = true;
  }
}

void Firebase::disable() {
  Debug("Firebase::disable\n");
  initialized = false;
}

void Firebase::loop() {
  if (!initialized) {
    return ;
  }

  // send a ping every PING_PERIOD
  unsigned long timeNow = now() * 1000; // don't use millis, it's 0 when starting :)
  if (config->getSendFirebasePing() && XUtils::isElapsedDelay(timeNow, &lastSendPing, PING_PERIOD)) {
    DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT + 5);
    JsonObject& jsonBufferRoot = jsonBuffer.createObject();
    // Let's flag the first ping after boot
    if (sendInitPing) {
      jsonBufferRoot["init"] = true;
      jsonBufferRoot["init_reason"] = ESP.getResetReason();
      sendInitPing = false;
    }
    jsonBufferRoot["failed_msg"] = failedMessageCount;
    jsonBufferRoot["retried_msg"] = retriedMessageCount;
    jsonBufferRoot["lost_msg"] = lostMessageCount;
    differMessage(MESSAGE_PING, &jsonBufferRoot);
  }

  handleDifferedMessages();
}

void Firebase::setCommonFields(JsonObject* jsonBufferRoot) {
  jsonBufferRoot->set("lang", XIOT_LANG);
  jsonBufferRoot->set("name", config->getName());
  jsonBufferRoot->set("mac", macAddrStr);
  char date[DATE_BUFFER_SIZE];
  jsonBufferRoot->set("date", getDateStr(date));
  uint32_t freeMem = system_get_free_heap_size();
  jsonBufferRoot->set("heap_size", freeMem);
}

void Firebase::differRecord(const char* type, JsonObject* jsonBufferRoot) {
  Debug("Firebase::sendRecord\n");
  setCommonFields(jsonBufferRoot);
  String serialized = "";
  jsonBufferRoot->printTo(serialized);
  differMessage(MESSAGE_MODULE, serialized);
}

int Firebase::sendEvent(const char* type, const char* logMessage) {
  char url[URL_MAX_LENGTH_WITHOUT_SECRET];
  sprintf(url, "%s/%s.json", config->getFirebaseUrl(), type);

  // If the message string contains Json, send it as it is
  if (strncmp(logMessage, "{\"", 2) == 0) {
    return sendToFirebase("POST", url, logMessage);
  } else {
    DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT + 1);
    JsonObject& jsonBufferRoot = jsonBuffer.createObject();
    jsonBufferRoot["message"] = logMessage;
    setCommonFields(&jsonBufferRoot);
    return sendToFirebase("POST", url, &jsonBufferRoot);
  }
}

char* Firebase::getDateStr(char* dateBuffer) {
  int h = hour();
  int mi = minute();
  int s = second();
  int d = day();
  int mo = month();
  int y= year();
  sprintf(dateBuffer, "%04d/%02d/%02dT%02d:%02d:%02d", y, mo, d, h, mi, s);
  return dateBuffer;
}

int Firebase::sendToFirebase(const char* method, const char* url, JsonObject* jsonBufferRoot) {
  Debug("Firebase::sendToFirebase json\n");
  int size = jsonBufferRoot->measureLength() + 3;
  Debug("Firebase json buffer size %d\n", size);
  char *buff = (char*)malloc(size);
  jsonBufferRoot->printTo(buff, size);
  
  int result = sendToFirebase(method, url, buff);
  free(buff);
  return result;
}

int Firebase::sendToFirebase(const char* method, const char* url, const char* payload) {
  Debug("Firebase::sendToFirebase string %s %s\n", method, url);
  //security. Should be useless since tested ealier to avoid useless json creation.
  if (!initialized) {
    return -1;
  }
  uint32_t freeMem = system_get_free_heap_size();
  Debug("Heap before sending to Firebase: %d\n", freeMem); 

// Debugging stuff
// Serial.println(payload);
// return -1;

  char urlWithSecret[URL_MAX_LENGTH_WITHOUT_SECRET + FIREBASE_SECRET_MAX_LENGTH];
  const char *token = config->getFirebaseSecretToken();
  // to disable token usage, set it to small string
  if (strlen(token) > 10) {
    sprintf(urlWithSecret, "%s?auth=%s", url, token);
  } else {
    strcpy(urlWithSecret, url);
  }

  int httpCode = XIOTHttps::sendToHttps(urlWithSecret, payload);
  freeMem = system_get_free_heap_size();
  Debug("Heap after sending to Firebase: %d\n", freeMem); 
  return httpCode;
}

// Trying to send a message while processing an incoming request crashes the module
// => handling a pile of messages that will be processed later, with retries.
void Firebase::differMessage(JsonObject* jsonBufferRoot) {
  differMessage(MESSAGE_LOG, jsonBufferRoot);
}

void Firebase::differMessage(MessageType type,  JsonObject* jsonBufferRoot) {
  String serialized = "";
  setCommonFields(jsonBufferRoot);
  jsonBufferRoot->printTo(serialized);
  differMessage(type, serialized);
}

void Firebase::differMessage(String message) {
  differMessage(MESSAGE_LOG, message);
}

void Firebase::differAlert(String message) {
  differMessage(MESSAGE_ALERT, message);
}

void Firebase::differMessage(MessageType type, String message) {
  Message **differedMessagePile = differedMessages;
  int count = 0;
  while (count < MAX_DIFFERED_MESSAGES_COUNT) {
    if (*differedMessagePile == NULL) {
      *differedMessagePile = new Message(type, message);
      Debug("Message added at position %d\n", count);
      break;
    }
    differedMessagePile ++;
    count ++;
  }
  if (count == MAX_DIFFERED_MESSAGES_COUNT) {
    Debug("Differed message pile full, message lost\n");    
    lostMessageCount ++;
  }
}

void Firebase::handleDifferedMessages() {
  if (!initialized || differedMessages[0] == NULL) {
    return;
  }
  unsigned long timeNow = millis();
  if(!XUtils::isElapsedDelay(timeNow, &lastHandledDifferedMessage, currentDifferedMessageDelay)) {
    return;
  }
  int httpCode = -1;
  differedMessages[0]->retryCount ++;
  differedMessages[0]->initialTime = timeNow;
  Debug("Sending message, try number %d\n", differedMessages[0]->retryCount);
  char url[URL_MAX_LENGTH_WITHOUT_SECRET];
  switch(differedMessages[0]->type) {
    case MESSAGE_LOG:
      httpCode = sendEvent("log", differedMessages[0]->message.c_str());
      break;

    case MESSAGE_ALERT:
      httpCode = sendEvent("alert", differedMessages[0]->message.c_str());
      break;

    case MESSAGE_PING:
      sprintf(url, "%s/ping.json", config->getFirebaseUrl());    
      httpCode = sendToFirebase("POST", url, differedMessages[0]->message.c_str());
      break;

    case MESSAGE_MODULE:
      sprintf(url, "%s/module/%s.json", config->getFirebaseUrl(), macAddrStr);    
      httpCode = sendToFirebase("PUT", url, differedMessages[0]->message.c_str());
      break;

    case MESSAGE_CUSTOM:
    case RECORD_CUSTOM:
    default:
      Debug("Message type not supported yet\n");
  }
  boolean deleteMessage = false;
  if (httpCode != 200) {
    if (differedMessages[0]->retryCount >= MAX_RETRY_MESSAGE_COUNT) {
      Debug("Differed message not sent, but already retried %d times: deleting\n", MAX_RETRY_MESSAGE_COUNT);
      deleteMessage = true;
      failedMessageCount ++;
    } else {
      Debug("Differed message not sent, will retry\n");
      retriedMessageCount ++;
      // Increase the retry delay if not already max
      if (currentDifferedMessageDelay < MAX_RETRY_DELAY) {
        currentDifferedMessageDelay += 250;
      }
    }
  } else {
    deleteMessage = true;
  }

  if (deleteMessage) {
    // Reset the retry delay
    currentDifferedMessageDelay = HANDLE_DIFFERED_MESSAGES_DEFAULT_DELAY_MS;
    // Shift the message fifo queue
    delete(differedMessages[0]);
    differedMessages[0] = NULL;
    for (int i=0; i < MAX_DIFFERED_MESSAGES_COUNT - 1; i++) {
      differedMessages[i] = differedMessages[i+1];
    }
    differedMessages[MAX_DIFFERED_MESSAGES_COUNT - 1] = NULL;
    // No more messages to send ?
    if (differedMessages[0] == NULL) {
      Debug("No more differed messages\n");
      failedMessageCount = 0;
      retriedMessageCount = 0;
      lostMessageCount = 0;
    }
  }
  

  
}