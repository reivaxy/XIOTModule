/**
 *  Firebase recording class
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include "Firebase.h"
#include <exception>


#define COMMON_FIELD_COUNT 3
#define PING_PERIOD 1000 * 60 * 5UL // 5mn
#define DATE_BUFFER_SIZE 25

#define URL_MAX_LENGTH_WITHOUT_SECRET 100

Firebase::Firebase(ModuleConfigClass* config) {
  this->config = config;
  for (int i=0; i < 9; i++) {
    differedMessages[i] = NULL;
  }
}

   
void Firebase::init() {
  Debug("Firebase::init\n");
  uint32_t freeMem = system_get_free_heap_size();
  Debug("Heap at begining of Firebase::init: %d\n", freeMem); 
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(macAddrStr, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
  lastSendPing = 0;

  if (strlen(config->getFirebaseUrl()) > 10) {
    initialized = true;
    // TODO handle authentication
    DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT + 4);
    JsonObject& jsonBufferRoot = jsonBuffer.createObject();
    const char* tu = config->getPushoverUser();
    const char* ta = config->getPushoverToken();
    // Tokens need to be provided, and ping activated. 
    // If ping is not activated, providing tokens will trigger notifications on Firebase, we don't want that
    if (config->getSendFirebasePing() && strlen(tu) > 10 && strlen(ta) > 10) {
      jsonBufferRoot["tu"] = tu;
      jsonBufferRoot["ta"] = ta;
    }
    jsonBufferRoot["lang"] = XIOT_LANG;
    jsonBufferRoot["type"] = config->getType();
    sendRecord("module", &jsonBufferRoot);
  }
}

void Firebase::reset() {
  Debug("Firebase::reset\n");
  initialized = false;
}

void Firebase::loop() {
  if (!initialized) {
    return ;
  }

  unsigned long timeNow = now() * 1000; // don't use millis, it's 0 when starting :)
  if (config->getSendFirebasePing() && XUtils::isElapsedDelay(timeNow, &lastSendPing, PING_PERIOD)) {
    DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT + 1);
    JsonObject& jsonBufferRoot = jsonBuffer.createObject();
    // Let's flag the first ping after boot
    if (millis() < 60000) {
      jsonBufferRoot["init"] = true;
    }
    sendEvent("ping", &jsonBufferRoot);
  }

  handleDifferedLogs();
}

int Firebase::sendAlert(const char* alertMessage) {
  return sendLog("alert", alertMessage);
}

int Firebase::sendLog(const char* logMessage) {
  return sendLog("log", logMessage);
}

int Firebase::sendLog(const char* type, const char* logMessage) {
  DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT + 2);
  JsonObject& jsonBufferRoot = jsonBuffer.createObject();
  jsonBufferRoot["message"] = logMessage;
  jsonBufferRoot["lang"] = XIOT_LANG;
  return sendEvent(type, &jsonBufferRoot);
}

void Firebase::setCommonFields(JsonObject *jsonBufferRoot) {
  jsonBufferRoot->set("name", config->getName());
  jsonBufferRoot->set("mac", macAddrStr);
  char date[DATE_BUFFER_SIZE];
  jsonBufferRoot->set("date", getDateStr(date));
}

int Firebase::sendRecord(const char* type, JsonObject* jsonBufferRoot) {
  Debug("Firebase::sendRecord\n");
  if (!initialized) {
    return -1;
  }
  setCommonFields(jsonBufferRoot);
  char url[URL_MAX_LENGTH_WITHOUT_SECRET];
  sprintf(url, "%s/%s/%s.json", config->getFirebaseUrl(), type, macAddrStr);
  sendToFirebase("PUT", url, jsonBufferRoot);
}

int Firebase::sendEvent(const char* type, JsonObject* jsonBufferRoot) {
  Debug("Firebase::sendEvent\n");
  if (!initialized) {
    return -1;
  }
  setCommonFields(jsonBufferRoot);
  char url[URL_MAX_LENGTH_WITHOUT_SECRET];
  sprintf(url, "%s/%s.json", config->getFirebaseUrl(), type);
  sendToFirebase("POST", url, jsonBufferRoot);
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
  Debug("Firebase::sendToFirebase json %s %s\n", method, url);
  int size = jsonBufferRoot->measureLength() + 3;
  Debug("Firebase buffer size %d\n", size);
  char *buff = (char*)malloc(size);
  jsonBufferRoot->printTo(buff, size);
  
  int result = sendToFirebase(method, url, buff);
  free(buff);
  return result;
}

int Firebase::sendToFirebase(const char* method, const char* url, char* payload) {
  Debug("Firebase::sendToFirebase string %s %s\n", method, url);
  uint32_t freeMem = system_get_free_heap_size();
  Debug("Heap before sending to Firebase: %d\n", freeMem); 
  // https requires a sh*tload of ram
  // if (freeMem < 32200) {
  //   Serial.println("Not enough memory to send request to firebase\n");
  //   return -3;
  // }
  char urlWithSecret[URL_MAX_LENGTH_WITHOUT_SECRET + FIREBASE_SECRET_MAX_LENGTH];
  const char *token = config->getFirebaseSecretToken();
  // to disable token usage, set it to small string
  if (strlen(token) > 10) {
    sprintf(urlWithSecret, "%s?auth=%s", url, token);
  } else {
    strcpy(urlWithSecret, url);
  }
  int httpCode = -1;
  WiFiClientSecure client;
  client.setInsecure();
  // use small buffers since payloads are small
  // Otherwise it will use almost 30K of ram and sometimes crash :(
  client.setBufferSizes(1024, 1024);  
  HTTPClient https;
  if (https.begin(client, urlWithSecret)) {
    // start connection and send  headers
    https.addHeader("Content-Type", "application/json");
    if(strcmp(method, "POST") == 0) {
      httpCode = https.POST((const uint8_t *)payload, strlen(payload));
    } else {
      httpCode = https.PUT((const uint8_t *)payload, strlen(payload));
    }
    Debug("HTTP response code: %d\n", httpCode);
    if (httpCode > 200) {
      Debug("%s\n", https.getString().c_str());
    }
    if (httpCode < 0) {
      Debug("%s\n", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Debug("Connection failed\n");
  }
  freeMem = system_get_free_heap_size();
  Debug("Heap after sending to Firebase: %d\n", freeMem); 
  return httpCode;
}

// Trying to send a message while processing an incoming request crashes the module
void Firebase::sendDifferedLog(const char* logMessage) {
  String **differedMessagePile = differedMessages;
  int count = 0;
  while (count <10) {
    if (*differedMessagePile == NULL) {
      *differedMessagePile = new String(logMessage);
      Debug("Differed message added at position %d\n", count);
      break;
    }
    differedMessagePile ++;
    count ++;
  }
  if (count == 10) {
    Debug("Differed message pile full, message lost\n");    
  }
}

void Firebase::handleDifferedLogs() {
  if (differedMessages[0] == NULL) {
    return;
  }
  sendLog(differedMessages[0]->c_str());
  delete(differedMessages[0]);
  for (int i=0; i < 9; i++) {
    differedMessages[i] = differedMessages[i+1];
  }
  differedMessages[9] = NULL;
}