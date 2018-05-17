/**
 *  Common stuff for iotinator master and slave modules 
 *  Xavier Grosjean 2018
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

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
};

#define CONFIG_PAYLOAD_SIZE 600

class XIOTModule {
public:
  XIOTModule(ModuleConfigClass* config, int addr, int sda, int scl);
  void refresh();
  DisplayClass* getDisplay();
  JsonObject& masterAPIGet(const char* path, int* httpCode);  
  
  
protected:
  void _connectSTA();  

  void _initDisplay(int displayAddr, int displaySda, int displayScl);
  void _timeDisplay();
  void _wifiDisplay();
  void _getConfigFromMaster();
  
  ModuleConfigClass* _config;
  DisplayClass* _oledDisplay;
  WiFiEventHandler _wifiSTAGotIpHandler, _wifiSTADisconnectedHandler;
  unsigned int _timeLastTimeDisplay = 0;
  unsigned int _timeLastGet = 0;
  bool _wifiConnected = false;
  bool _canGet = false;
  bool _timeInitialized = false;  
  char *_localIP;
};