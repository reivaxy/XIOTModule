/**
 *  Common stuff for iotinator master and agent modules 
 *  Xavier Grosjean 2021
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#pragma once


#include "XIOTModuleDebug.h"
#include <XIOTConfig.h>
#include <XUtils.h>
#include <WString.h>
#include <pgmspace.h>
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
#include "Firebase.h"

// Try to minimise RAM size and fragmentation: store as many strings as possible in PROGMEM
static const char MSG_OTA_READY_IP[] PROGMEM = "OTA ready: %s";
static const char MSG_MAC_ADDRESS[] PROGMEM = "%02x:%02x:%02x:%02x:%02x:%02x";
static const char MSG_OTA_PROGRESS[] PROGMEM = "Progress: %u%%";
static const char MSG_OTA_ERROR[] PROGMEM = "OTA Error[%u]: ";
static const char MSG_OTA_AUTH_FAILED_ERROR[] PROGMEM = "Auth Failed";
static const char MSG_OTA_BEGIN_FAILED_ERROR[] PROGMEM = "Begin Failed";
static const char MSG_OTA_CONNECT_FAILED_ERROR[] PROGMEM = "Connect Failed";
static const char MSG_OTA_RECEIVE_FAILED_ERROR[] PROGMEM = "Receive Failed";
static const char MSG_OTA_END_FAILED_ERROR[] PROGMEM = "End Failed";

static const char MSG_RESTARTING[] PROGMEM = "restarting";
static const char MSG_OTA_READY[] PROGMEM = "Ready for OTA";
static const char MSG_CONFIG_SAVED[] PROGMEM = "Config saved";

static const char JSON_EMPTY[] PROGMEM = "{}";


static const char RENAMING_AGENT_TO[] PROGMEM = "Renaming agent to %s\n";
static const char DATE_TIME_FORMAT[] PROGMEM = "%02d:%02d:%02d %02d/%02d/%04d";
static const char IP_SSID_FORMAT[] PROGMEM = "%s (%s)";
static const char CONNECTING_SSID_FORMAT[] PROGMEM = "Connecting to %s";
static const char INT_FORMAT[] PROGMEM = "%d";

static const char SERVER_ARG_PLAIN[] PROGMEM = "plain";
static const char SERVER_ARG_AUTONOMOUS[] PROGMEM = "autonomous";
static const char SERVER_ARG_PING[] PROGMEM = "ping";
static const char SERVER_ARG_NAME[] PROGMEM = "name";
static const char SERVER_ARG_POUSER[] PROGMEM = "poUser";
static const char SERVER_ARG_POTOKEN[] PROGMEM = "poToken";
static const char SERVER_ARG_FIREBASEURL[] PROGMEM = "firebaseUrl";
static const char SERVER_ARG_FIREBASESECRET[] PROGMEM = "firebaseSecret";
static const char SERVER_ARG_XIOTPASSWD[] PROGMEM = "xiotPwd";
static const char SERVER_ARG_XIOTSSID[] PROGMEM = "xiotSsid";
static const char SERVER_ARG_BOXPASSWD[] PROGMEM = "boxPwd";
static const char SERVER_ARG_BOXSSID[] PROGMEM = "boxSsid";
static const char SERVER_ARG_TIMEOFFSET[] PROGMEM = "timeOffset";
static const char SERVER_ARG_BRIGHTNESS[] PROGMEM = "brightness";

static const char SERVER_HEADER_FORWARD[] PROGMEM = "Xiot-forward-to";
static const char SERVER_HEADER_CONNECTION[] PROGMEM = "Connection";
static const char SERVER_HEADER_CONNECTION_VALUE_CLOSE[] PROGMEM = "close";
static const char SERVER_HEADER_CONTENTTYPE_VALUE_HTML[] PROGMEM = "text/html; charset=utf-8";
static const char SERVER_HEADER_CONTENTTYPE_VALUE_JSON[] PROGMEM = "application/json";
static const char SERVER_HEADER_CONTENTTYPE_VALUE_TEXT[] PROGMEM = "text/plain";


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
  virtual void setCustomModuleRecordFields(JsonObject *jsonBufferRoot);
  
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
  void sendText(const __FlashStringHelper * str, int code);
  void sendHtml(const char* msg, int code);
  void sendHtml(const __FlashStringHelper * str, int code);
  void sendJson(const char* msg, int code);
  void sendJson(const __FlashStringHelper * str, int code);
  void sendEmptyJson(int code);
  void hideDateTime(bool);
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
  void setStackStart(char* stackStart);
  void processNtpEvent();
  bool isTimeInitialized();
  int sendPushNotif(const char* title, const char* message);
  void sendModuleInfo();
  String getServerArg(const __FlashStringHelper * str);
  String getServerForwardHeader();
  void sendConnectionClose();

  Firebase* firebase = NULL;

  bool _hideDateTime = false;
  ModuleConfigClass* _config;
  bool _otaIsStarted = false;
  time_t _otaReadyTime = 0;
  DisplayClass* _oledDisplay;
  ESP8266WebServer* _server;
  WiFiEventHandler _wifiSTAGotIpHandler, _wifiSTADisconnectedHandler;
  unsigned int _timeLastTimeDisplay = 0;
  unsigned long _timeLastRegister = 0;
  unsigned long _timeLastGetConfig = 0;
  unsigned long lastSendPing = 0;
  bool _wifiConnected = false;
  bool _canQueryMasterConfig = false;
  bool _canRegister = false;
  bool _timeInitialized = false;  
  bool _refreshNeeded = false;  
  char _localIP[IP_MAX_LENGTH + 1];
  bool _ntpEventToProcess = false;
  bool _ntpServerInitialized = false;
  bool _ntpListenerInitialized = false;
  char macAddrStr[MAC_ADDR_MAX_LENGTH + 1];

  NTPSyncEvent_t _ntpEvent;
};