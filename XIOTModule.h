/**
 *  Common stuff for iotinator master and slave modules 
 *  Xavier Grosjean 2018
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#pragma once

#include <XIOTConfig.h>
#include <XIOTDisplay.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <TimeLib.h>
#include <XUtils.h>
extern "C" {
  #include "user_interface.h"  // Allow getting heap size
}

//#define DEBUG_XIOTMODULE // Uncomment this to enable debug messages over serial port

#ifdef DEBUG_XIOTMODULE
#define Debug(...) Serial.printf(__VA_ARGS__)
#else
#define Debug(...)
#endif

// Max length authorized for modules custom data
#define MAX_CUSTOM_DATA_SIZE 200
// String used to replace a too long custom value
#define CUSTOM_DATA_TOO_BIG_VALUE "CUSTOM_DATA_TOO_BIG_REMOVED"

#define JSON_STRING_SMALL_SIZE 500
#define JSON_STRING_MEDIUM_SIZE 3000
#define JSON_STRING_BIG_SIZE 10000

#define JSON_BUFFER_CONFIG_SIZE JSON_OBJECT_SIZE(40)
#define JSON_STRING_CONFIG_SIZE 1000

#define JSON_BUFFER_REGISTER_SIZE JSON_OBJECT_SIZE(20)
#define JSON_STRING_REGISTER_SIZE 1000 + MAX_CUSTOM_DATA_SIZE


class XIOTModuleJsonTag {
public:
  static const char* timestamp;
  static const char* APInitialized;
  static const char* version;
  static const char* APSsid;
  static const char* APPwd;
  static const char* homeWifiConnected;
  static const char* gsmEnabled;
  static const char* timeInitialized;
  static const char* name;
  static const char* ip;
  static const char* MAC;
  static const char* canSleep;
  static const char* uiClassName;
  static const char* custom;
  static const char* pong;
};

#define IP_MAX_LENGTH 16
#define DOUBLE_IP_MAX_LENGTH 32  // will be handy when slaves can also open AP
#define MAC_ADDR_MAX_LENGTH 18

class XIOTModule {
public:
  XIOTModule(DisplayClass *display);
  XIOTModule(ModuleConfigClass* config, int addr, int sda, int scl);
  void refresh();
  DisplayClass* getDisplay();
  ESP8266WebServer* getServer();
  void masterAPIGet(const char* path, int* httpCode, char *jsonString, int maxLen);  
  void masterAPIPost(const char* path, String payload, int* httpCode, char *jsonString = NULL, int maxLen = 0);
  void APIGet(String ipAddr, const char* path, int* httpCode, char *jsonString, int maxLen);  
  void APIGet(String ipAddr, const char* path, int* httpCode);  
  void APIPost(String ipAddr, const char* path, String payload, int* httpCode, char *jsonString, int maxLen);  
  void APIPost(String ipAddr, const char* path, String payload, int* httpCode);  
  void sendText(const char* msg, int code);
  void sendHtml(const char* msg, int code);
  void sendJson(const char* msg, int code);
  
protected:
  void _connectSTA();  

  void _initDisplay(int displayAddr, int displaySda, int displayScl);
  void _initServer();
  void _timeDisplay();
  void _wifiDisplay();
  void _getConfigFromMaster();
  void _register();
  virtual char* _customRegistrationData();
  
  ModuleConfigClass* _config;
  DisplayClass* _oledDisplay;
  ESP8266WebServer* _server;
  WiFiEventHandler _wifiSTAGotIpHandler, _wifiSTADisconnectedHandler;
  unsigned int _timeLastTimeDisplay = 0;
  unsigned int _timeLastRegister = 0;
  unsigned int _timeLastGetConfig = 0;
  bool _wifiConnected = false;
  bool _canQueryMasterConfig = false;
  bool _canRegister = false;
  bool _timeInitialized = false;  
  char *_localIP;
};