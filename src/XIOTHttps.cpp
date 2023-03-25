
/**
 *  Utility class to handle HTTPS requests
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include "XIOTHttps.h"
#include <XUtils.h>


#define RESPONSE_READ_YIELD_PERIOD_MS 1000
#define RESPONSE_READ_TIMEOUT_MS 6000
#define READ_BUFFER_SIZE 300


int XIOTHttps::sendToHttps(const char* method, const char* url, const char* payload) {
   return sendToHttps(method, url, payload, 2048, 3000);
}

// Only for PUT and POST for now.
int XIOTHttps::sendToHttps(const char* method, const char* url, const char* payload, int bufferSize, int timeout) {
  MemSize("XIOTHttps::sendToHttps");
  int httpCode = -1;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(timeout);
  Debug("XIOTHttps::sendToHttps payload size %d\n", strlen(payload));
  // use small buffers since payloads are small
  // Otherwise it will use almost 30K of ram and sometimes crash :(
  client.setBufferSizes(bufferSize, bufferSize);  

  char *urlCopy = XUtils::mallocAndCopy(url);
  char *firstPathSlash = strchr(urlCopy + 9, '/');
  *firstPathSlash = 0;
  char *host = urlCopy + 8;
  //Debug("%s on host %s\n", method, host);
  // Serial.printf("Url: %s\n", url);
  // Serial.printf("Host: %s\n", host);
  //delay(0);
  if (!client.connect(host, 443)) {
    Serial.println("Connection failed!");
  } else {
    Serial.println("Connected!");
    client.printf("%s %s HTTP/1.1\n", method, url);
    client.printf("Host: %s\n", host);
    client.println("Content-Type: application/json");
    client.printf("Content-Length: %d\n", strlen(payload));
    client.println("Connection: close");
    client.println();  
    client.println(payload);
    // read output
    // First line is result:
    // HTTP/1.1 401 Unauthorized
    // HTTP/1.1 200 OK
    // Next lines are response headers until empty line
    Debug("Response:\n");
    bool firstLine = true;
    bool readTimeout = false;
    unsigned long startReadingResponse = millis();
    unsigned long sinceLastYield = startReadingResponse;

    char line[READ_BUFFER_SIZE + 1];
 
    while (client.connected()) {
      size_t readCount = client.readBytesUntil('\n', line, READ_BUFFER_SIZE);
      line[readCount] = 0;
      if (firstLine) {
        Debug("%s\n", line);
        char *code = line+9;
        code[3] = 0;
        httpCode = atoi(code);
        Debug("Http code: %d\n", httpCode);
        firstLine = false;
      }
      if (strcmp(line,"\r")) {
        break;
      }

      if (XUtils::isElapsedDelay(millis(), &startReadingResponse, RESPONSE_READ_TIMEOUT_MS)) {
        Debug("Timeout while reading response headers\n");
        readTimeout = true;
        break;
      }
      // if (XUtils::isElapsedDelay(millis(), &sinceLastYield, RESPONSE_READ_YIELD_PERIOD_MS)) {
      //   yield();
      // }
    }
    // we are done reading the headers. There can be a body
    // Don't know if reading them is mandatory if we don't need them?
    // if there are incoming bytes available
    // from the server, read them
    // while (!readTimeout && client.connected()) {
    //   char c = client.read();
    //   if (XUtils::isElapsedDelay(millis(), &startReadingResponse, RESPONSE_READ_TIMEOUT_MS)) {
    //     Debug("Timeout while reading response body\n");
    //     break;
    //   }
    //   if (XUtils::isElapsedDelay(millis(), &sinceLastYield, RESPONSE_READ_YIELD_PERIOD_MS)) {
    //     yield();
    //   }
    // }
    client.stop();
    Serial.printf("HTTP response code %d\n", httpCode);

  }

  free(urlCopy);
  return httpCode;
}

void XIOTHttps::yield() {
  delay(0);
}