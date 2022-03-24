/**
 *  Common stuff for iotinator master and agent modules 
 *  Xavier Grosjean 2021
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
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NtpClientLib.h>


extern "C" {
  #include "user_interface.h"  // Allow getting heap size
}

#define DEBUG_XIOTMODULE // Uncomment this to enable debug messages over serial port

#ifdef DEBUG_XIOTMODULE
#define Debug(...) Serial.printf(__VA_ARGS__)
#else
#define Debug(...)
#endif

// Max length authorized for modules custom data
#define MAX_GLOBAL_STATUS_SIZE 30
// String used to replace a too long global status
#define GLOBAL_STATUS_TOO_BIG_VALUE "GLOBAL_STATUS_TOO_BIG"  // needs to be less than MAX_GLOBAL_STATUS_SIZE-1 characters

#define MAX_CUSTOM_DATA_SIZE 200
// String used to replace a too long custom value
#define CUSTOM_DATA_TOO_BIG_VALUE "CUSTOM_DATA_TOO_BIG_REMOVED"

#define JSON_STRING_SMALL_SIZE 500
#define JSON_STRING_MEDIUM_SIZE 3000
#define JSON_STRING_BIG_SIZE 10000

// https://arduinojson.org/v5/assistant says  JSON_OBJECT_SIZE(8) + 140
#define JSON_BUFFER_CONFIG_SIZE JSON_OBJECT_SIZE(15) + 200

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
  static const char* globalStatus;
  static const char* connected;
  static const char* heap;
  static const char* pingPeriod;
  static const char* registeringTime;
  static const char* pwd;
  static const char* ssid;
};

#define IP_MAX_LENGTH 16
#define DOUBLE_IP_MAX_LENGTH 32  // will be handy when slaves can also open AP
#define MAC_ADDR_MAX_LENGTH 18

class XIOTModule {
// TODO: sort out public/protected stuff, for now it does not really make any sense
public:
  XIOTModule(DisplayClass *display, bool flipScreen = true, uint8_t brightness = 100);
  XIOTModule(ModuleConfigClass* config, int addr, int sda, int scl, bool flipScreen = true, uint8_t brightness = 255);
  virtual void loop();
  virtual void customLoop();
  virtual void customRegistered(bool isSuccess);
  virtual void customGotConfig(bool isSuccess);
  virtual bool customBeforeOTA();
  virtual void customOnStaGotIpHandler(WiFiEventStationModeGotIP ipInfo);
  virtual char* customFormInitPage();
  virtual char* customPageInitPage();
  virtual int customSaveConfig();

  virtual bool customProcessSMS(const char* phoneNumber, const bool isAdmin, const char* message);
  virtual int sendData(bool isResponse);
  virtual void _timeDisplay();
  virtual void _wifiDisplay();
  virtual void _getConfigFromMaster();
  virtual void _register();
  virtual char* _customData();
  virtual char* _globalStatus();
  virtual char* useData(const char* data, int* responseCode);
  virtual char* emptyMallocedResponse();
  virtual int _refreshMaster();
  
  DisplayClass* getDisplay();
  ESP8266WebServer* getServer();
  void masterAPIGet(const char* path, int* httpCode, char *jsonString, int maxLen);  
  void masterAPIPost(const char* path, String payload, int* httpCode, char *jsonString = NULL, int maxLen = 0);
  void APIGet(String ipAddr, const char* path, int* httpCode, char *jsonString, int maxLen);  
  void APIGet(String ipAddr, const char* path, int* httpCode);  
  void APIPut(String ipAddr, const char* path, String payload, int* httpCode, char *jsonString, int maxLen);  
  void APIPost(String ipAddr, const char* path, String payload, int* httpCode, char *jsonString, int maxLen);  
  void APIPost(char *ipAddr, const char* path, String payload, int* httpCode, char *jsonString, int maxLen);  
  void APIPost(String ipAddr, const char* path, String payload, int* httpCode);  
  void sendText(const char* msg, int code);
  void sendHtml(const char* msg, int code);
  void sendJson(const char* msg, int code);
  void hideDateTime(bool);
  bool _hideDateTime = false;
  void addModuleEndpoints();
  bool isWaitingOTA();
  int startOTA(const char* ssid, const char*pwd);
  void _initSoftAP();
  void waitForOta();
  void _connectSTA();  
  void _processPostPut();
  void _setupOTA();
  char* _buildFullPayload();
  void _initDisplay(int displayAddr, int displaySda, int displayScl, bool flipScreen = true, uint8_t brightness = 100);
  void _processSMS();
  const char* getSTASsid();
  const char* getSTAPwd();
  void processNtpEvent();
  bool isTimeInitialized();
  int sendToHttps(const char* url, const char* payload);
  int sendPushNotif(const char* title, const char* message);
 
  ModuleConfigClass* _config;
  bool _otaIsStarted = false;
  time_t _otaReadyTime = 0;
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
  bool _refreshNeeded = false;  
  char _localIP[IP_MAX_LENGTH + 1];
  bool _ntpEventToProcess = false;
  bool _ntpServerInitialized = false;
  bool _ntpListenerInitialized = false;

  NTPSyncEvent_t _ntpEvent;
};