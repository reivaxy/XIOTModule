
/**
 *  Utility class to handle HTTPS requests
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include "XIOTHttps.h"

int XIOTHttps::sendToHttps(const char* url, const char* payload) {
   return sendToHttps(url, payload, 2048, 3000);
}

int XIOTHttps::sendToHttps(const char* url, const char* payload, int bufferSize, int timeout) {
  int httpCode = -1;
  WiFiClientSecure client;
  client.setTimeout(timeout);
  client.setInsecure();
  // use small buffers since payloads are small
  // Otherwise it will use almost 30K of ram and sometimes crash :(
  client.setBufferSizes(bufferSize, bufferSize);  
  HTTPClient https;
  if (https.begin(client, url)) {
    // start connection and send  headers
    https.addHeader("Content-Type", "application/json");
    httpCode = https.POST((const uint8_t *)payload, strlen(payload));
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