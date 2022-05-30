
/**
 *  Utility class to handle HTTPS requests
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#include "XIOTHttps.h"
#include <XUtils.h>

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

  // Serial.printf("Url: %s\n", url);
  // Serial.printf("Host: %s\n", host);

  if (!client.connect(host, 443)) {
    Serial.println("Connection failed!");
  } else {
    client.printf("%s %s HTTP/1.1\n", method, url);
    client.printf("Host: %s\n", host);
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.printf("Content-Length: %d\n", strlen(payload));
    client.println();  
    client.println(payload);

    // read output
    // First line is result:
    // HTTP/1.1 401 Unauthorized
    // HTTP/1.1 200 OK
    // Next lines are response headers until empty line
    Debug("Response:\n");
    bool firstLine = true;

    // TODO: handle timeout ?
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (firstLine) {
        httpCode = line.substring(9, 12).toInt();
        firstLine = false;
      }
      Debug("%s\n", line.c_str());
      if (line == "\r") {
        break;
      }
    }
    // Don't know if reading them is mandatory if we don't need them?
    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client.available()) {
      char c = client.read();
      //Serial.write(c);
    }
    client.stop();
    Serial.printf("HTTP response code %d\n", httpCode);

  }

  free(urlCopy);
  return httpCode;
}