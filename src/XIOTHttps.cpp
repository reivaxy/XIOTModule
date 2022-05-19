
/**
 *  Utility class to handle HTTPS requests
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include "XIOTHttps.h"

int XIOTHttps::sendToHttps(const char* method, const char* url, const char* payload) {
   return sendToHttps(method, url, payload, 2048, 5000);
}

int XIOTHttps::sendToHttps(const char* method, const char* url, const char* payload, int bufferSize, int timeout) {
  MemSize("before sending https request");
  int httpCode = -1;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(timeout);
  // use small buffers since payloads are small
  // Otherwise it will use almost 30K of ram and sometimes crash :(
  client.setBufferSizes(bufferSize, bufferSize);  
  HTTPClient https;
  if (https.begin(client, url)) {
    // start connection and send  headers
    https.addHeader("Content-Type", "application/json");
    if (strncmp("POST", method, 4) == 0) {
      Debug("POST\n");
      httpCode = https.POST((const uint8_t *)payload, strlen(payload));
    } else {
      Debug("PUT\n");
      httpCode = https.PUT((const uint8_t *)payload, strlen(payload));
    }
    MemSize("after sending https request");
    Debug("HTTP response code: %d\n", httpCode);
    if (httpCode > 200) {
      Serial.println(https.getString().c_str());
    }
    if (httpCode < 0) {
      Serial.println(https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    Serial.println("Connection failed\n");
  }
  return httpCode;
}