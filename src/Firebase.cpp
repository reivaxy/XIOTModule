/**
 *  Firebase recording class
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include "Firebase.h"

#define COMMON_FIELD_COUNT 3
#define PING_PERIOD 1000 * 60 * 5UL // 5mn
#define DATE_BUFFER_SIZE 25

#define URL_MAX_LENGTH_WITHOUT_SECRET 300

Firebase::Firebase(ModuleConfigClass* config) {
  this->config = config;
}

   
void Firebase::init() {
  Debug("Firebase::init\n");
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
    DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT);
    JsonObject& jsonBufferRoot = jsonBuffer.createObject();
    sendEvent("ping", &jsonBufferRoot);
  }
}

int Firebase::sendAlert(const char* alertMessage) {
  return sendLog(alertMessage, "alert");
}

int Firebase::sendLog(const char* logMessage) {
  return sendLog(logMessage, "log");
}

int Firebase::sendLog(const char* logMessage, const char* type) {
  DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT + 1);
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
  int size = jsonBufferRoot->measureLength() + 1;
  char *buff = (char*)malloc(size);
  jsonBufferRoot->printTo(buff, size);
  int result = sendToFirebase(method, url, buff);
  free(buff);
  return result;
}

int Firebase::sendToFirebase(const char* method, const char* url, const char* payload) {
  Debug("Firebase::sendToFirebase string %s %s\n", method, url);
  char urlWithSecret[URL_MAX_LENGTH_WITHOUT_SECRET + FIREBASE_SECRET_MAX_LENGTH];
  const char *token = config->getFirebaseSecretToken();
  // to disable token usage, set it to small string
  if (strlen(token) > 10) {
    sprintf(urlWithSecret, "%s?auth=%s", url, token);
  } else {
    strcpy(urlWithSecret, url);
  }
  int httpCode = -1;
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  if (https.begin(*client, urlWithSecret)) {
    // start connection and send  headers
    https.addHeader("Content-Type", "application/json");
    if(strcmp(method, "POST") == 0) {
      httpCode = https.POST((const uint8_t *)payload, strlen(payload));
    } else {
      httpCode = https.PUT((const uint8_t *)payload, strlen(payload));
    }
    Debug("HTTP response code: %d\n", httpCode);
    if (httpCode > 200) {
      Debug(https.getString().c_str());
    }
    if (httpCode < 0) {
      Debug(https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Debug("Connection failed");
  }
  return httpCode;
}