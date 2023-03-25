
/**
 *  Utility class to handle HTTPS requests
 *  Xavier Grosjean 2022
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "XIOTModuleDebug.h"

static const char request[] PROGMEM = "%s %s HTTP/1.1\nHost: %s\nContent-Type: application/json\nContent-Length: %d\nConnection: close\n\n%s";

class XIOTHttps {
   public:

   static int sendToHttps(const char* method, const char* url, const char* payload);
   static int sendToHttps(const char* method, const char* url, const char* payload, int bufferSize, int timeout);
   static void yield();

};