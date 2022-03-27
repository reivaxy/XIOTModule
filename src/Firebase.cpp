/**
 *  Firebase recording class
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include <XIOTConfig.h>
#include "Firebase.h"

#define COMMON_FIELD_COUNT 4
#define PING_PERIOD 1000 * 60 * 5UL // 5mn
#define DATE_BUFFER_SIZE 25


Firebase::Firebase(ModuleConfigClass* config) {
  this->config = config;
}

   
void Firebase::init() {
  Debug("Firebase::init\n");
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(macAddrStr, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
  if (strlen(config->getFirebaseUrl()) > 10) {
    initialized = true;
    lastSendPing = millis();
    // TODO handle authentication
    DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT + 2);
    JsonObject& jsonBufferRoot = jsonBuffer.createObject();
    const char* tu = config->getPushoverUser();
    const char* ta = config->getPushoverToken();
    if (strlen(tu) > 10 && strlen(ta) > 10) {
      jsonBufferRoot["tu"] = tu;
      jsonBufferRoot["ta"] = ta;
    }
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
  unsigned long timeNow = millis();
  if (config->getSendPing() && XUtils::isElapsedDelay(timeNow, &lastSendPing, PING_PERIOD)) {
    if(initialized) {
      DynamicJsonBuffer jsonBuffer(COMMON_FIELD_COUNT);
      JsonObject& jsonBufferRoot = jsonBuffer.createObject();
      sendEvent("ping", &jsonBufferRoot);

    }
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
  return sendEvent(type, &jsonBufferRoot);
}

void Firebase::setCommonFields(JsonObject *jsonBufferRoot) {
  jsonBufferRoot->set("name", config->getName());
  jsonBufferRoot->set("timestamp", now() - 60 * config->getGmtMinOffset());
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
  char url[300];
  sprintf(url, "%s/%s/%s.json", config->getFirebaseUrl(), type, macAddrStr);
  sendToHttps("PUT", url, jsonBufferRoot);
}

int Firebase::sendEvent(const char* type, JsonObject* jsonBufferRoot) {
  if (!initialized) {
    return -1;
  }
  setCommonFields(jsonBufferRoot);
  char url[300];
  sprintf(url, "%s/%s.json", config->getFirebaseUrl(), type);
  sendToHttps("POST", url, jsonBufferRoot);
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

int Firebase::sendToHttps(const char* method, const char* url, JsonObject* jsonBufferRoot) {
  Debug("Firebase::sendToHttps json %s %s\n", method, url);
  int size = jsonBufferRoot->measureLength() + 1;
  char *buff = (char*)malloc(size);
  jsonBufferRoot->printTo(buff, size);
  int result = sendToHttps(method, url, buff);
  Debug("%s\n", buff);
  free(buff);
  return result;
}

int Firebase::sendToHttps(const char* method, const char* url, const char* payload) {
  Debug("Firebase::sendToHttps string %s %s\n", method, url);
  int httpCode = -1;
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  if (https.begin(*client, url)) {
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