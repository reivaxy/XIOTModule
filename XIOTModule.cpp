

#include "XIOTModule.h"

const char* XIOTModuleJsonTag::timestamp = "timestamp";
const char* XIOTModuleJsonTag::APInitialized = "APInitialized";
const char* XIOTModuleJsonTag::version = "version";
const char* XIOTModuleJsonTag::APSsid = "APSsid";
const char* XIOTModuleJsonTag::APPwd = "APPwd";
const char* XIOTModuleJsonTag::ssid = "ssid";
const char* XIOTModuleJsonTag::pwd = "pwd";
const char* XIOTModuleJsonTag::homeWifiConnected = "homeWifiConnected";
const char* XIOTModuleJsonTag::gsmEnabled = "gsmEnabled";
const char* XIOTModuleJsonTag::timeInitialized = "timeInitialized";
const char* XIOTModuleJsonTag::name = "name";
const char* XIOTModuleJsonTag::ip = "ip";
const char* XIOTModuleJsonTag::MAC = "MAC";
const char* XIOTModuleJsonTag::canSleep = "canSleep";
const char* XIOTModuleJsonTag::uiClassName = "uiClassName";
const char* XIOTModuleJsonTag::custom = "custom";
const char* XIOTModuleJsonTag::globalStatus = "globalStatus";
const char* XIOTModuleJsonTag::connected = "connected";
const char* XIOTModuleJsonTag::heap = "heap";
const char* XIOTModuleJsonTag::pingPeriod = "pingPeriod";
const char* XIOTModuleJsonTag::registeringTime = "regTime";

/**
 * This constructor is used by master iotinator, just to take advantage of
 * some methods available here. It's crappy, need to be fixed
 */
XIOTModule::XIOTModule(DisplayClass *display, bool flipScreen, uint8_t brightness) {
  WiFi.mode(WIFI_OFF);  // Make sure reconnection will be handled properly after reset
//  _setupOTA();
  _oledDisplay = display;
  _server = new ESP8266WebServer(80);
}

/**
 * This constructor is used only by agent modules that take full advantage of this class
 */
XIOTModule::XIOTModule(ModuleConfigClass* config, int displayAddr, int displaySda, int displayScl, bool flipScreen, uint8_t brightness) {
  WiFi.mode(WIFI_OFF);  // Make sure reconnection will be handled properly after reset
//  _setupOTA();
  _config = config;
  Serial.print("Initializing module ");
  Serial.println(config->getName());

  // Initialise the OLED display
  _initDisplay(displayAddr, displaySda, displayScl, flipScreen, brightness);
  if(config->getUiClassName()[0] == 0) {
    Serial.println("No uiClassName !!");
    _oledDisplay->setLine(2, "No uiClassName !", NOT_TRANSIENT, NOT_BLINKING);
    _oledDisplay->alertIconOn(true);
  }  
  
  // Initialize the web server for the API
  _server = new ESP8266WebServer(80);

  addModuleEndpoints();
  
  // Nb: & allows to keep the reference to the caller object in the lambda block
  _wifiSTAGotIpHandler = WiFi.onStationModeGotIP([&](WiFiEventStationModeGotIP ipInfo) {
    free(_localIP);
    XUtils::stringToCharP(ipInfo.ip.toString(), &_localIP);
    if(isWaitingOTA()) {
      char message[30];
      sprintf(message, "OTA ready: %s", _localIP);
      _oledDisplay->setLine(0, message, NOT_TRANSIENT, NOT_BLINKING);
      ArduinoOTA.begin();    
    } else {
      Serial.printf("Got IP on %s: %s\n", _config->getSsid(), _localIP);
      _wifiConnected = true;
      _canQueryMasterConfig = true;
      _wifiDisplay();
      
      // If connected to the customized SSID, module can register itself to master
      if(strcmp(DEFAULT_APPWD, _config->getPwd()) != 0) {
        _canRegister = true;
      }
      customOnStaGotIpHandler(ipInfo);
    }
  }); 
  
  _wifiSTADisconnectedHandler = WiFi.onStationModeDisconnected([&](WiFiEventStationModeDisconnected event) {
    // Continuously get messages, so just output once.
    if(_wifiConnected && !isWaitingOTA() ) {
      Serial.printf("Lost connection to %s, error: %d\n", event.ssid.c_str(), event.reason);
      _oledDisplay->setLine(1, "Disconnected", TRANSIENT, NOT_BLINKING);
      _connectSTA();
    }
  });

  // Module is Wifi Station only
  WiFi.mode(WIFI_STA);  
  _connectSTA();
}

ESP8266WebServer* XIOTModule::getServer() {
  return _server;
}

void XIOTModule::addModuleEndpoints() {
  // list of headers we want to be able to read
  const char * headerkeys[] = {"Xiot-forward-to"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  _server->collectHeaders(headerkeys, headerkeyssize );
    
  _server->on("/api/ping", HTTP_GET, [&]() {
    sendData(true);    
  });

  _server->on("/api/moduleReset", HTTP_GET, [&](){
    Serial.println("Rq on /api/moduleReset");
    _config->initFromDefault();
    _config->saveToEeprom();
    sendJson("{}", 200);   // HTTP code 200 is enough 
  });

  _server->on("/api/rename", HTTP_POST, [&]() {
    String forwardTo = _server->header("Xiot-forward-to");   // when an agent can be a proxy to other agents
    String jsonBody = _server->arg("plain");
    if(forwardTo.length() != 0) { 
      int httpCode;   
      Serial.print("Forwarding rename to ");
      Serial.println(forwardTo);
      // TODO process error
      APIPost(forwardTo, "/api/rename", jsonBody, &httpCode, NULL, 0);
    } else {
      if(_config == NULL) {
        sendJson("{\"error\": \"No config to update.\"}", 404);
        return;
      }
  
      char message[100];
      const int bufferSize = JSON_OBJECT_SIZE(2) + 20 + NAME_MAX_LENGTH;
      StaticJsonBuffer<bufferSize> jsonBuffer; 
      JsonObject& root = jsonBuffer.parseObject(jsonBody); 
      if (!root.success()) {
        sendJson("{}", 500);
        _oledDisplay->setLine(1, "Renaming agent failed", TRANSIENT, NOT_BLINKING);
        return;
      }
      sprintf(message, "Renaming agent to %s\n", (const char*)root["name"] ); 
      _oledDisplay->setLine(1, message, TRANSIENT, NOT_BLINKING);
      _config->setName((const char*)root["name"]);
      _config->saveToEeprom(); // TODO: partial save !!   
      _oledDisplay->setTitle(_config->getName());
    }    
    sendJson("{}", 200);   // HTTP code 200 is enough
  });

  // Return this module's custom data if any
  // Almost like ping request except for heap size. Is it worth it ? Could be exact same... 
  _server->on("/api/data", HTTP_GET, [&]() {
    String forwardTo = _server->header("Xiot-forward-to");
    int httpCode;
    if(forwardTo.length() != 0) {    
      Serial.print("Forwarding GET /api/data to ");
      Serial.println(forwardTo);
      char message[1000];
      APIGet(forwardTo, "/api/data", &httpCode, message, 1000);
      if(httpCode == 200) {
        sendJson(message, httpCode);
      }
    } else {  
      sendData(true);
    }
  });
  
  // The BackBone framework uses PUT to save data from UI to modules  
  _server->on("/api/data", HTTP_PUT, [&]() {
    _processPostPut();
  });
  
  // But the modules can't PUT, they POST: handle both
  _server->on("/api/data", HTTP_POST, [&]() {
    _processPostPut();
  });
      
  _server->on("/api/sms", HTTP_POST, [&]() {
    _processSMS();
  });
      
  _server->on("/api/restart", HTTP_GET, [&](){
    String forwardTo = _server->header("Xiot-forward-to");
    int httpCode;
    if(forwardTo.length() != 0) {    
      Serial.print("Forwarding restart to ");
      Serial.println(forwardTo);
      APIGet(forwardTo, "/api/restart", &httpCode, NULL, 0);
    } else {
      ESP.restart();     
    }
  });
  
  // OTA: update. NB: for now, master has its own api endpoint 
  _server->on("/api/ota", HTTP_POST, [&]() {
    String jsonBody = _server->arg("plain");
    int httpCode = 200;
    char ssid[SSID_MAX_LENGTH];
    char pwd[PWD_MAX_LENGTH];
    _oledDisplay->init(); 
    _oledDisplay->setLineAlignment(2, TEXT_ALIGN_CENTER);
  
    const int bufferSize = JSON_OBJECT_SIZE(2) + 15 + SSID_MAX_LENGTH + PWD_MAX_LENGTH;
    StaticJsonBuffer<bufferSize> jsonBuffer;   
    JsonObject& root = jsonBuffer.parseObject(jsonBody); 
    if (!root.success()) {
      sendJson("{}", 500);
      _oledDisplay->setLine(1, "Ota setup failed", TRANSIENT, NOT_BLINKING);
      return;
    }
    const char *ssidp = (const char*)root[XIOTModuleJsonTag::ssid];
    const char *pwdp = (const char*)root[XIOTModuleJsonTag::pwd];
    sendJson("{}", httpCode); // send reply before disconnecting/reconnecting.     
    httpCode = startOTA(ssidp, pwdp);
  });
    
  _server->begin();
}    

// This is responding to api/ping and api/data (for GET symmetry with put/post on api/data)
// This is also when refreshing data: not responding to a request but posting to master.
int XIOTModule::sendData(bool isResponse) {
  int httpCode = 200;
  char *payloadStr = _buildFullPayload();
  if(isResponse) {
    Serial.printf("Response: %s\n", payloadStr);
    sendJson(payloadStr, httpCode);
  } else {
    Serial.printf("Payload: %s\n", payloadStr);
    masterAPIPost("/api/refresh", String(payloadStr), &httpCode, NULL, 0);
  }  
  free(payloadStr);
  return httpCode;
}

void XIOTModule::_processPostPut() {  
  String forwardTo = _server->header("Xiot-forward-to");
  String body = _server->arg("plain");
  int httpCode;
  char *bodyStr, *response;
  XUtils::stringToCharP(body, &bodyStr);
  
  if(forwardTo.length() != 0) {    
    Serial.print("Forwarding data to ");
    Serial.println(forwardTo);
    response = (char *)malloc(1000);
    *response = 0;
    APIPost(forwardTo, "/api/data", body, &httpCode, response, 1000);
  } else {
    response = useData(bodyStr, &httpCode);  // Each module subclass should override this if it expects any data from the UI.
    // For now the response can't be used by master to update its agent collecting
    // This will be done when master subclasses XIOTModule... 
    // So for now we'll refresh the data on master after this request callback is done
    _refreshNeeded = true;      
  }

  if(response == NULL) {
    response = _buildFullPayload();
  }
  sendJson(response, httpCode);
  free(response);        
  free(bodyStr);   
}

void XIOTModule::_processSMS() {
  String jsonBody = _server->arg("plain");
  Serial.println(jsonBody); 
  const int bufferSize = JSON_OBJECT_SIZE(3) ; 
  StaticJsonBuffer<bufferSize> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(const_cast<char*>(jsonBody.c_str())); 
  if (!root.success()) {
    sendJson("{}", 500);
    _oledDisplay->setLine(1, "SMS bad payload", TRANSIENT, NOT_BLINKING);
    return;
  }
  const char *message = (const char*)root["message"];       
  const char *phoneNumber = (const char*)root["phoneNumber"];       
  const bool isAdmin = (const bool)root["isAdmin"];
         
  bool success = customProcessSMS(phoneNumber, isAdmin, message);
  if(success) {
    char *payload = _buildFullPayload();
    sendJson(payload, 200);
    free(payload);
  } else {
    sendText("", 500);
  } 

}    


/**
 * Connects to the SSID read in config
 */
void XIOTModule::_connectSTA() {
  _canQueryMasterConfig = false;
  _canRegister = false;
  _wifiConnected = false;
  Debug("XIOTModule::_connectSTA %s\n", _config->getSsid());
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
  _oledDisplay->setLine(1, "Getting config...", TRANSIENT, NOT_BLINKING);
  masterAPIGet("/api/config", &httpCode, jsonString, JSON_STRING_CONFIG_SIZE);
  StaticJsonBuffer<JSON_BUFFER_CONFIG_SIZE>  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(jsonString);
  if(httpCode == 200) {
    _canQueryMasterConfig = false;
    _oledDisplay->setLine(1, "Got config", TRANSIENT, NOT_BLINKING);
    customGotConfig(true);
  } else {
    _oledDisplay->setLine(1, "Getting config failed", TRANSIENT, NOT_BLINKING);
    customGotConfig(false);
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
  _oledDisplay->setLine(1, "Registering", TRANSIENT, NOT_BLINKING);
  _wifiDisplay();
  char* payload = _buildFullPayload();
    
  //Serial.println(message);
  masterAPIPost("/api/register", payload, &httpCode);
  if(httpCode == 200) {
    _canRegister = false;
    _oledDisplay->setLine(1, "Registered", TRANSIENT, NOT_BLINKING);
    customRegistered(true);
  } else {
    _oledDisplay->setLine(1, "Registration failed", TRANSIENT, NOT_BLINKING);
    customRegistered(false);
  }
  free(payload);
}

/**
 * Returns a malloced string with config
 * Caller needs to free it
 */
char* XIOTModule::_buildFullPayload() {
  char macAddrStr[20];
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(macAddrStr, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
  StaticJsonBuffer<JSON_BUFFER_CONFIG_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root[XIOTModuleJsonTag::name] = _config->getName();
  root[XIOTModuleJsonTag::ip] = _localIP;
  root[XIOTModuleJsonTag::MAC] = macAddrStr;
  root[XIOTModuleJsonTag::uiClassName] = _config->getUiClassName();
  char *globalStatus = _globalStatus();
  if(globalStatus) {  
    root[XIOTModuleJsonTag::globalStatus] = globalStatus;
  }
  uint32_t freeMem = system_get_free_heap_size();
  Serial.printf("Free heap mem: %d\n", freeMem);
  root[XIOTModuleJsonTag::heap] = freeMem;

  // When implemented: return true if module uses sleep feature (battery)
  // So that master won't ping
  // TODO: handle this in config like getUiClassName
  root[XIOTModuleJsonTag::canSleep] = false;

  // The customPayload is the module's data that will be available to the webApp
  // It's sent to the master when registering, as a JSON string contained in the
  // "custom" attribute of the JSON registration payload.
  // Sending it as a string means the master won't have trouble computing the Jsonbuffer size
  // since it's just one element instead of an object containing many elements.
  // The customdata will also be sent in response to the ping request from master
  char *customPayload = _customData();
  if(customPayload != NULL) {
    if(strlen(customPayload) < MAX_CUSTOM_DATA_SIZE) {
      root[XIOTModuleJsonTag::custom] = customPayload;
    } else {
      _oledDisplay->setLine(1, "Custom Data too big", TRANSIENT, NOT_BLINKING);
      root[XIOTModuleJsonTag::custom] = CUSTOM_DATA_TOO_BIG_VALUE;
    }
  }
  char* payload = (char*)malloc(JSON_STRING_CONFIG_SIZE);   // TODO: improve ?
  root.printTo(payload, JSON_STRING_CONFIG_SIZE);
  free(customPayload);
  free(globalStatus);
  return payload;
}

// Use this method to refresh the module's data on master
// It's the data the UI is polling
int XIOTModule::_refreshMaster() {
  return sendData(false);
}

// This method should be overloaded in modules that need to provide custom info at registration time
// and in response to GET /api/ping, and in response to GET /api/data
char* XIOTModule::_customData() {
  return NULL;
}

// This method should be overloaded in modules that need to provide a global status (OK, ALERT, ON, OFF...)
char* XIOTModule::_globalStatus() {
  return NULL;
}

// This method should be overloaded in modules that need to process SMS messages
bool XIOTModule::customProcessSMS(const char* phoneNumber, const bool isAdmin, const char* message) {
  return true;
}

// This method should be overloaded in modules that need to process info from the UI
// (sent by POST /get/data) 
// Needs to return a malloc'ed char *
char* XIOTModule::useData(char* data, int* httpCode) {
  *httpCode = 200;
  return emptyMallocedResponse();
}

char* XIOTModule::emptyMallocedResponse() {
  char* dummy = (char*) malloc(5);
  strcpy(dummy, "{}");
  return dummy;
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
 * Send a GET request to given IP, handling only the response code 
 */
void XIOTModule::APIGet(String ipAddr, const char* path, int* httpCode) {
  return APIGet(ipAddr, path, httpCode, NULL, 0);
}

/**
 * Send a GET request to given IP, handling response code and payload 
 */

void XIOTModule::APIGet(String ipAddr, const char* path, int* httpCode, char *jsonString, int maxLen) {
  Debug("XIOTModule::APIGet\n");
  HTTPClient http;
  http.begin(ipAddr, 80, path);
  *httpCode = http.GET();
  if(*httpCode <= 0) {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(*httpCode).c_str());
    return;
  }
  if(jsonString) {
    String jsonResultStr = http.getString();
    strlcpy(jsonString, jsonResultStr.c_str(), maxLen);
  }
  http.end();
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
void XIOTModule::APIPost(char* ipAddr, const char* path, String payload, int* httpCode, char *jsonString, int maxLen) {
  String ipAddrString = String(ipAddr);
  APIPost(ipAddrString, path, payload, httpCode, jsonString, maxLen);
}

void XIOTModule::APIPost(String ipAddr, const char* path, String payload, int* httpCode, char *jsonString, int maxLen) {
  Debug("XIOTModule::APIPost\n");
//  Serial.println(ipAddr);
  Serial.println(path);
  HTTPClient http;
  http.begin(ipAddr, 80, path);
  *httpCode = http.POST(payload);
  if(*httpCode <= 0) {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(*httpCode).c_str());
    return;
  }
  if(jsonString) {
    String jsonResultStr = http.getString();
    strlcpy(jsonString, jsonResultStr.c_str(), maxLen);
  }
  http.end();
}

bool XIOTModule::isWaitingOTA() {
  return (_otaReadyTime != 0);
}

// Set the module in Update Waiting Mode: connect to ssid if provided, and setup the stuff
void XIOTModule::_setupOTA() {
  ArduinoOTA.onStart([&]() {
    _otaIsStarted = true;
    _oledDisplay->setLine(1, "Loading...", NOT_TRANSIENT, BLINKING);
    _oledDisplay->setLine(2, "Start updating", TRANSIENT, NOT_BLINKING);
  });  
  ArduinoOTA.onEnd([&]() {
    _oledDisplay->setLine(2, "End updating", TRANSIENT, NOT_BLINKING);
  });  
  ArduinoOTA.onProgress([&](unsigned int progress, unsigned int total) {
    char message[50];
    if(progress == total) {
      _oledDisplay->setLine(1, "Flashing...", NOT_TRANSIENT, BLINKING);
    }
    sprintf(message, "Progress: %u%%", (progress / (total / 100)));
    _oledDisplay->setLine(2, message, NOT_TRANSIENT, NOT_BLINKING);
  
    _oledDisplay->refresh();
  });
  ArduinoOTA.onError([&](ota_error_t error) {
    char msgErr[50];

    sprintf(msgErr, "OTA Error[%u]: ", error);
    _oledDisplay->setLine(1, msgErr, NOT_TRANSIENT, BLINKING);
     
    if (error == OTA_AUTH_ERROR) sprintf(msgErr,"Auth Failed");
    else if (error == OTA_BEGIN_ERROR) sprintf(msgErr,"Begin Failed");
    else if (error == OTA_CONNECT_ERROR) sprintf(msgErr,"Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) sprintf(msgErr,"Receive Failed");
    else if (error == OTA_END_ERROR) sprintf(msgErr,"End Failed");
    _oledDisplay->setLine(1, msgErr, NOT_TRANSIENT, NOT_BLINKING);
  });
}

int XIOTModule::startOTA(const char* ssid, const char* pwd) {
  bool enabled = customBeforeOTA();
  Serial.printf("SSID : %s\n", ssid);
  if(!enabled) {
    _oledDisplay->setLine(1, "OTA mode refused", TRANSIENT, NOT_BLINKING);
    return 403;  
  }
  _otaReadyTime = millis();
  _setupOTA();
  _oledDisplay->setLine(0, "Switching SSID for OTA", NOT_TRANSIENT, BLINKING);
  _oledDisplay->setLine(1, "Waiting for OTA", NOT_TRANSIENT, BLINKING);
  _oledDisplay->setLine(2, "", NOT_TRANSIENT, NOT_BLINKING);
  if(ssid != NULL && strlen(ssid) > 0) {
    Serial.printf("Connecting to %s\n", ssid);
    WiFi.begin(ssid, pwd);
  }
  return 200;
}

void XIOTModule::_initDisplay(int displayAddr, int displaySda, int displayScl, bool flipScreen, uint8_t brightness) {
  Debug("XIOTModule::_initDisplay\n");
  _oledDisplay = new DisplayClass(displayAddr, displaySda, displayScl, flipScreen, brightness);
  _oledDisplay->setTitle(_config->getName());
  _wifiDisplay();
  _timeDisplay();
}

void XIOTModule::_timeDisplay() {
//  Debug("XIOTModule::_timeDisplay\n");
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
  if(isWaitingOTA() ) return;
  
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


/**
 * This method MUST be called in your sketch main loop method
 * to properly handle clock, display, server
 * Or you need to handle these by yourself. 
 */
void XIOTModule::loop() {
  now(); // Needed to update the clock from the TimeLib library
  // (and used by NTP library)
      
  // Check if any request to serve
  _server->handleClient();
    
  if(isWaitingOTA()) {  
    int remainingTime = 180 - ((millis() - _otaReadyTime) / 1000);
    char remainingTimeMsg[10];
    sprintf(remainingTimeMsg, "%d", remainingTime);
    _oledDisplay->setLine(4, remainingTimeMsg);
    // If waiting for OTA for more than 3mn but not started, restart (cancel OTA)
    if(!_otaIsStarted && ( remainingTime <= 0)) {
      _oledDisplay->setLine(1, "OTA cancelled (timeout)");
      _oledDisplay->refresh();
      delay(200);
      ESP.restart();
      return; // probably not necessary
    }
    _oledDisplay->refresh();
    ArduinoOTA.handle();
    return;
  }
  
  unsigned int timeNow = millis();
  // Should we get the config from master ?
  if(_wifiConnected && _canQueryMasterConfig && (timeNow - _timeLastGetConfig >= 5000)) {
    _timeLastGetConfig = timeNow;
    _getConfigFromMaster();
    _oledDisplay->refresh();
    delay(300); // Otherwise message can't be read !
  }
  if(_wifiConnected && _canRegister && (timeNow - _timeLastRegister >= 5000)) {
    _timeLastRegister = timeNow;
    _register(); 
    _oledDisplay->refresh();
    delay(300); // Otherwise message can't be read !
  } 
  
  // Time on display should be refreshed every second
  // Intentionnally not using the value returned by now(), since it changes
  // when time is set.
  if(timeNow - _timeLastTimeDisplay >= 1000) {
    _timeLastTimeDisplay = timeNow;
    _timeDisplay();
  }
  if(_refreshNeeded) {
    int httpCode = _refreshMaster();
    // TODO: make sure we don't retry this every loop cycle
    // if(httpCode == 200) {
      _refreshNeeded = false;
    // }
  }
  
  customLoop();
  
  // Display needs to be refreshed continuously (for blinking, ...)
  _oledDisplay->refresh();    
}

void XIOTModule::hideDateTime(bool flag) {
  _oledDisplay->hideDateTime(flag);
}

void XIOTModule::customLoop() {
  // Override this method to implement your recurring processes
}

void XIOTModule::customGotConfig(bool isSuccess) {
  // Override this method to implement your post getConfig init process
}

void XIOTModule::customRegistered(bool isSuccess) {
  // Override this method to implement your post registration init process
}
void XIOTModule::customOnStaGotIpHandler(WiFiEventStationModeGotIP ipInfo) {
  // Override this method to implement process once module is connected
}

bool XIOTModule::customBeforeOTA() {
  // Override this method to implement custom processing before initiating OTA.
  // You can block it returning false.
  return true;
}




