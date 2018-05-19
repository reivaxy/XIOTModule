

#include "XIOTModule.h"

const char* XIOTModuleJsonTag::timestamp = "timestamp";
const char* XIOTModuleJsonTag::APInitialized = "APInitialized";
const char* XIOTModuleJsonTag::version = "version";
const char* XIOTModuleJsonTag::APSsid = "APSsid";
const char* XIOTModuleJsonTag::APPwd = "APPwd";
const char* XIOTModuleJsonTag::homeWifiConnected = "homeWifiConnected";
const char* XIOTModuleJsonTag::gsmEnabled = "gsmEnabled";
const char* XIOTModuleJsonTag::timeInitialized = "timeInitialized";
const char* XIOTModuleJsonTag::name = "name";
const char* XIOTModuleJsonTag::slaveIP = "slaveIP";
const char* XIOTModuleJsonTag::MAC = "MAC";
const char* XIOTModuleJsonTag::canSleep = "canSleep";
const char* XIOTModuleJsonTag::uiClassName = "uiClassName";
const char* XIOTModuleJsonTag::custom = "custom";

/**
 * This constructor is used by master iotinator, just to take advantage of
 * some methods available here.
 * Later, the class will be able to handle STA_AP
 */
XIOTModule::XIOTModule(DisplayClass *display) {
  _oledDisplay = display;
  _initServer();
}

/**
 * This constructor is used only by slave modules that take full advantage of this class
 */
XIOTModule::XIOTModule(ModuleConfigClass* config, int displayAddr, int displaySda, int displayScl) {
  _config = config;
  Serial.print("Initializing module ");
  Serial.println(config->getName());
    
  // Initialise the OLED display
  _initDisplay(displayAddr, displaySda, displayScl);
  
  if(config->getUiClassName()[0] == 0) {
    Serial.println("No uiClassName !!");
    _oledDisplay->setLine(2, "No uiClassName !", NOT_TRANSIENT, NOT_BLINKING);
    _oledDisplay->alertIconOn(true);
  }  
  
  // Initialize the web server for the API
  _initServer();
    
  // Module is Wifi Station only
  WiFi.mode(WIFI_STA);
  
  // Nb: & allows to keep the reference to the caller object in the lambda block :)
  _wifiSTAGotIpHandler = WiFi.onStationModeGotIP([&](WiFiEventStationModeGotIP ipInfo) {
    XUtils::stringToCharP(ipInfo.ip.toString(), &_localIP);
    Serial.printf("Got IP on %s: %s\n", _config->getSsid(), _localIP);
    _wifiConnected = true;
    _canQueryMasterConfig = true;  // Can't perform an http get from within the handler, it fails...
    _wifiDisplay();
    
    // If connected to the customized SSID, module can register itself to master
    if(strcmp(DEFAULT_APPWD, _config->getPwd()) != 0) {
      _canRegister = true;
    }
  }); 
  
  _wifiSTADisconnectedHandler = WiFi.onStationModeDisconnected([&](WiFiEventStationModeDisconnected event) {
    // Continuously get messages, so just output once.
    if(_wifiConnected) {
      free(_localIP);
      Serial.printf("Lost connection to %s, error: %d\n", event.ssid.c_str(), event.reason);
      _wifiConnected = false;
      _wifiDisplay();
    }
  });
  _connectSTA();
}

ESP8266WebServer* XIOTModule::getServer() {
  return _server;
}

void XIOTModule::_initServer() {
  _server = new ESP8266WebServer(80);
  
  _server->on("/api/ping", [&](){
    sendJson("{}", 200);   // HTTP code 200 is enough
  });

  _server->on("/reset", [&](){
    Serial.println("Rq on /reset XIOTModule");
    _config->initFromDefault();
    _config->saveToEeprom();
    sendJson("{}", 200);   // HTTP code 200 is enough 
  });

  _server->on("/api/rename", [&](){
    Serial.println("Renaming");
    String jsonBody = _server->arg("plain");
    char message[100];
    StaticJsonBuffer<100> jsonBuffer; 
    JsonObject& root = jsonBuffer.parseObject(jsonBody); 
    if (!root.success()) {
      Serial.println("Renaming failure");
      sendJson("{}", 500);
      _oledDisplay->setLine(1, "Renaming failed", TRANSIENT, NOT_BLINKING);
      return;
    }
    Serial.printf("Renaming to %s\n", (const char*)root["name"] ); 
    _config->setName((const char*)root["name"]);
    _config->saveToEeprom(); // TODO: partial save !!   
    _oledDisplay->setTitle(_config->getName());
    sendJson("{}", 200);   // HTTP code 200 is enough
  });
    
  _server->begin();
}

/**
 * Connects to the SSID read in config
 */
void XIOTModule::_connectSTA() {
  _canQueryMasterConfig = false;
  _canRegister = false;
  Debug("XIOTModule::_connectSTA\n");
  WiFi.begin(_config->getSsid(), _config->getPwd());
  _wifiDisplay();
}


DisplayClass* XIOTModule::getDisplay() {
  return _oledDisplay;
}

void XIOTModule::_getConfigFromMaster() {
  Debug("XIOTModule::_getConfigFromMaster\n");
  int httpCode = 0;
  char jsonString[JSON_STRING_CONFIG_SIZE + 1]; 
  masterAPIGet("/api/config", &httpCode, jsonString, JSON_STRING_CONFIG_SIZE);
  StaticJsonBuffer<JSON_BUFFER_CONFIG_SIZE>  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(jsonString);
  if(httpCode == 200) {
    _canQueryMasterConfig = false;
  } else {
    Serial.println("Registration failed");
    return;
  }
    
  bool masterTimeInitialized = root[XIOTModuleJsonTag::timeInitialized];
  
  if(masterTimeInitialized) {
    long timestamp = root[XIOTModuleJsonTag::timestamp];
    setTime(timestamp);
    _timeInitialized = true;
  }
  bool APInitialized = root[XIOTModuleJsonTag::APInitialized];
  // If access point on Master was customized, get its ssid and password, 
  // Save them in EEProm
  if(APInitialized) {
    const char *ssid = root[XIOTModuleJsonTag::APSsid];   
    const char *pwd = root[XIOTModuleJsonTag::APPwd];
    // If AP not same as the one in config, save it
    if(strcmp(pwd, _config->getPwd()) != 0) {
      _config->setSsid(ssid);
      _config->setPwd(pwd);
      _config->saveToEeprom();     // TODO: partial save only !!!
      _connectSTA();      
    }
  }    
}

/**
 * Register the module to the master.
 * Send IP address, name, ...
 */
void XIOTModule::_register() {
  int httpCode;
  char message[JSON_STRING_CONFIG_SIZE];
  char macAddrStr[100];
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(macAddrStr, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
  
  StaticJsonBuffer<JSON_BUFFER_CONFIG_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject(); 
  root[XIOTModuleJsonTag::name] = _config->getName();
  root[XIOTModuleJsonTag::slaveIP] = _localIP;
  root[XIOTModuleJsonTag::MAC] = macAddrStr;
  root[XIOTModuleJsonTag::uiClassName] = _config->getUiClassName();
  // When implemented: return true if module uses sleep feature (battery)
  // So that master won't ping
  root[XIOTModuleJsonTag::canSleep] = false;
  
  char *customPayload = _customRegistrationData();
  if(customPayload != NULL) {
    // Yes we add a string (could be some serialized JSON), that can be stored in master's slave collection
    root[XIOTModuleJsonTag::custom] = customPayload;
  }  
  root.printTo(message, JSON_STRING_CONFIG_SIZE);
  if(customPayload != NULL) {
    free(customPayload);
  }
  //Serial.println(message);
  masterAPIPost("/api/register", message, &httpCode);
  if(httpCode == 200) {
    _canRegister = false;
  } else {
    Serial.println("Registration failed");
  } 
}

// This class should be overloaded in modules that need to provide custom info at registration time
// Not sure yet there is a need for that... 
char* XIOTModule::_customRegistrationData() {
  return NULL;
}

/**
 * Send a GET request to master 
 * Returns received json
 */
void XIOTModule::masterAPIGet(const char* path, int* httpCode, char *jsonString, int maxLen) {
  Debug("XIOTModule::masterAPIGet\n");
  String masterIP = WiFi.gatewayIP().toString();
  return APIGet(masterIP, path, httpCode, jsonString, maxLen);
}

/**
 * Send a GET request to given IP 
 */
void XIOTModule::APIGet(String ipAddr, const char* path, int* httpCode) {
  return APIGet(ipAddr, path, httpCode, NULL, 0);
}
void XIOTModule::APIGet(String ipAddr, const char* path, int* httpCode, char *jsonString, int maxLen) {
  Debug("XIOTModule::APIGet\n");
  HTTPClient http;
  http.begin(ipAddr, 80, path);
  *httpCode = http.GET();
  if(*httpCode <= 0) {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(*httpCode).c_str());
    return;
  }
  String jsonResultStr = http.getString();
  http.end();
  if(jsonString) {
    strlcpy(jsonString, jsonResultStr.c_str(), maxLen);
  }
}

/**
 * Send a POST request to master 
 * Returns received json
 */
void XIOTModule::masterAPIPost(const char* path, String payload, int* httpCode, char *jsonString, int maxLen) {
  Debug("XIOTModule::masterAPIPost\n");
  String masterIP = WiFi.gatewayIP().toString();
  return APIPost(masterIP, path, payload, httpCode, jsonString, maxLen);
}

/**
 * Send a POST request to given IP 
 * Returns received json
 */
void XIOTModule::APIPost(String ipAddr, const char* path, String payload, int* httpCode) {
  return APIPost(ipAddr, path, payload, httpCode, NULL, 0);
}
void XIOTModule::APIPost(String ipAddr, const char* path, String payload, int* httpCode, char *jsonString, int maxLen) {
  Debug("XIOTModule::APIPost\n");
  HTTPClient http;
  http.begin(ipAddr, 80, path);
  *httpCode = http.POST(payload);
  if(*httpCode <= 0) {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(*httpCode).c_str());
    return;
  }
  String jsonResultStr = http.getString();
  http.end();
  if(jsonString) {
    strlcpy(jsonString, jsonResultStr.c_str(), maxLen);
  }
}


void XIOTModule::_initDisplay(int displayAddr, int displaySda, int displayScl) {
  Debug("XIOTModule::_initDisplay\n");
  _oledDisplay = new DisplayClass(displayAddr, displaySda, displayScl);
  _oledDisplay->setTitle(_config->getName());
  _wifiDisplay();
  _timeDisplay();
}

void XIOTModule::_timeDisplay() {
  Debug("XIOTModule::_timeDisplay\n");
  _oledDisplay->clockIcon(!_timeInitialized);
  if(_timeInitialized) {
    int h = hour();
    int mi = minute();
    int s = second();
    int d = day();
    int mo = month();
    int y= year();
    char message[30];
    sprintf(message, "%02d:%02d:%02d %02d/%02d/%04d", h, mi, s, d, mo, y);
    _oledDisplay->refreshDateTime(message);
  }
}

void XIOTModule::_wifiDisplay() {
  Debug("XIOTModule::_wifiDisplay\n");
  char message[100];
  WifiType wifiType = STA;
  bool blinkWifi = false;

  if(_wifiConnected) {
    strcpy(message, _config->getSsid());
    strcat(message, " ");
    strcat(message, _localIP);
  } else {
    sprintf(message, "Connecting to %s", _config->getSsid());
  }
  _oledDisplay->setLine(0, message);
  
  if (!_wifiConnected) {
    blinkWifi = true;
  }
  _oledDisplay->wifiIcon(blinkWifi, wifiType);
}

/**
 * This method MUST be called in your sketch main loop method
 * to properly handle clock, display, server
 * Or you need to handle these by yourself. 
 */
void XIOTModule::refresh() {
  now(); // Needed to update the clock from the TimeLib library
  // (and used by NTP library)
      
  // Check if any request to serve
  _server->handleClient();  
 
  unsigned int timeNow = millis();
  // Should we get the config from master ?
  if(_wifiConnected && _canQueryMasterConfig && (timeNow - _timeLastGetConfig >= 5000)) {
    _timeLastGetConfig = timeNow;
    _getConfigFromMaster();
  }
  if(_wifiConnected && _canRegister && (timeNow - _timeLastRegister >= 5000)) {
    _timeLastRegister = timeNow;
    _register(); 
  } 
  
  // Time on display should be refreshed every second
  // Intentionnally not using the value returned by now(), since it changes
  // when time is set.
  if(timeNow - _timeLastTimeDisplay >= 1000) {
    _timeLastTimeDisplay = timeNow;
    _timeDisplay();
  }
 
  // Display needs to be refreshed continuously (for blinking, ...)
  _oledDisplay->refresh();    
}

void XIOTModule::sendHtml(const char* html, int code) {
  _server->sendHeader("Connection", "close");
  _server->send(code, "text/html", html);
}

void XIOTModule::sendJson(const char* jsonText, int code) {
  _server->sendHeader("Connection", "close");
  _server->send(code, "application/json", jsonText);
}
void XIOTModule::sendText(const char* msg, int code) {
  _server->sendHeader("Connection", "close");
  _server->send(code, "text/plain", msg);
}






