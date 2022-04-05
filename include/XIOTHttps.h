
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

class XIOTHttps {
   public:

   static int sendToHttps(const char* url, const char* payload);
   static int sendToHttps(const char* url, const char* payload, int bufferSize, int timeout);

};