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
#include <TimeLib.h>

//#define DEBUG_XIOTMODULE // Uncomment this to enable debug messages over serial port

#ifdef DEBUG_XIOTMODULE
#define Debug(...) Serial.printf(__VA_ARGS__)
#else
#define Debug(...)
#endif

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
  static const char* slaveIP;
};

#define CONFIG_PAYLOAD_SIZE 600
#define IP_MAX_LENGTH 16
#define DOUBLE_IP_MAX_LENGTH 32  // will be handy when slaves can also open AP

class XIOTModule {
public:
  XIOTModule(DisplayClass *display);
  XIOTModule(ModuleConfigClass* config, int addr, int sda, int scl);
  void refresh();
  DisplayClass* getDisplay();
  ESP8266WebServer* getServer();
  JsonObject& masterAPIGet(const char* path, int* httpCode);  
  JsonObject& masterAPIPost(const char* path, String payload, int* httpCode);
  JsonObject& APIGet(String ipAddr, const char* path, int* httpCode);  
  JsonObject& APIPost(String ipAddr, const char* path, String payload, int* httpCode);  
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