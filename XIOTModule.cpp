

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
const char* XIOTModuleJsonTag::connected = "connected";
const char* XIOTModuleJsonTag::heap = "heap";
const char* XIOTModuleJsonTag::pingPeriod = "pingPeriod";
const char* XIOTModuleJsonTag::registeringTime = "regTime";

/**
 * This constructor is used by master iotinator, just to take advantage of
 * some methods available here. It's crappy, need to be fixed
 */
//XIOTModule::XIOTModule(DisplayClass *display, bool flipScreen, uint8_t brightness) {
//  WiFi.mode(WIFI_OFF);  // Make sure reconnection will be handled properly after reset
//  _setupOTA();
//  _oledDisplay = display;
//  _server = new ESP8266WebServer(80);
//}


XIOTModule::XIOTModule(ModuleConfigClass* config, int displayAddr, int displaySda, int displayScl, bool flipScreen, uint8_t brightness) {
  WiFi.mode(WIFI_OFF);  // Make sure reconnection will be handled properly after reset
  _setupOTA();
  _config = config;
  Serial.print("Initializing module ");
  Serial.println(config->getName());

  // Initialise the OLED display
  _initDisplay(displayAddr, displaySda, displayScl, flipScreen, brightness);
  
  #ifndef XIOT_MASTER
  if(config->getUiClassName()[0] == 0) {
    Serial.println("No uiClassName !!");
    _oledDisplay->setLine(2, "No uiClassName !", NOT_TRANSIENT, NOT_BLINKING);
    _oledDisplay->alertIconOn(true);
  }  
  #endif

  #ifdef XIOT_MASTER
  // Initialize the Agent Collection
  agentCollection = new AgentCollection(this);
  #endif
  
    
  // Initialize the web server for the API
  _server = new ESP8266WebServer(80);

  addModuleEndpoints();
  
  // Nb: [&] allows to keep the reference to the caller object in the lambda block
  _wifiSTAGotIpHandler = WiFi.onStationModeGotIP([&](WiFiEventStationModeGotIP ipInfo) {
    free(_localIP);
    XUtils::stringToCharP(ipInfo.ip.toString(), &_localIP);
    if(_otaIsStarted) {
      char message[30];
      sprintf(message, "Ota ready: %s", _localIP);
      _oledDisplay->setLine(0, message, NOT_TRANSIENT, NOT_BLINKING);    
    } else {
      Serial.printf("Got IP on %s: %s\n", _config->getSsid(), _localIP);
      _wifiConnected = true;
      _canQueryMasterConfig = true;
      _wifiDisplay();
      
      // If connected to the customized SSID, module can register itself to master
      if(strcmp(DEFAULT_APPWD, _config->getPwd()) != 0) {
        _canRegister = true;
      }
    }
  }); 
  
  _wifiSTADisconnectedHandler = WiFi.onStationModeDisconnected([&](WiFiEventStationModeDisconnected event) {
    // Continuously get messages, so just output once.
    if(_wifiConnected && !_otaIsStarted ) {
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

  server->on("/api/rename", HTTP_POST, [&]() {
    char *forwardTo;
    XUtils::stringToCharP(server->header("Xiot-forward-to"), &forwardTo);
    String jsonBody = server->arg("plain");
    char message[100];
    
    // I've seen a few unexplained parsing error so I have set a bigger buffer size...
    // TODO : fix this size
    const int bufferSize = 2* JSON_OBJECT_SIZE(2);
    StaticJsonBuffer<bufferSize> jsonBuffer; 
    JsonObject& root = jsonBuffer.parseObject(jsonBody); 
    if (!root.success()) {
      sendJson("{}", 500);
      if(strlen(forwardTo) != 0) { 
       _oledDisplay->setLine(1, "Renaming agent failed", TRANSIENT, NOT_BLINKING);
      } else {
       _oledDisplay->setLine(1, "Renaming master failed", TRANSIENT, NOT_BLINKING);
      }
      free(forwardTo);
      return;
    }
    // Forward the rename to an agent
    if(strlen(forwardTo) != 0) {
      #ifdef XIOT_MASTER     
      agentCollection->renameAgent(forwardTo, (const char*)root["name"]);
      #endif
      // TODO: warn if not a master
      free(forwardTo);
    } else {
      free(forwardTo);    
      if(config == NULL) {
        sendJson("{\"error\": \"No config to update.\"}", 404);
        return;
      }
      sprintf(message, "Renaming to %s\n", (const char*)root["name"] ); 
     _oledDisplay->setLine(1, message, TRANSIENT, NOT_BLINKING);
      config->setName((const char*)root["name"]);
      config->saveToEeprom(); // TODO: partial save !!   
     _oledDisplay->setTitle(config->getName());
    }    
    sendJson("{}", 200);   // HTTP code 200 is enough
  });  

#ifdef XIOT_MASTER
  server->on("/api/list", HTTP_GET, []() {
    int size = agentCollection->getCount();
    int customStrSize = 0;
    
    // Size estimation: https://arduinojson.org/assistant/
    // TODO: update this when necessary : 10 fields per agent (for now it's actually 8)
    const size_t bufferSize = size*JSON_OBJECT_SIZE(10)
                              + JSON_OBJECT_SIZE(size)
                              +  + JSON_OBJECT_SIZE(1) ;
    
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root = jsonBuffer.createObject();    
    JsonObject& agentList = root.createNestedObject("agentList");
    
    agentCollection->list(agentList, &customStrSize);
 
    char* strBuffer = (char *)malloc(customStrSize); 
    root.printTo(strBuffer, customStrSize-1);
    Serial.printf("Reserved size: %d, actual size: %d\n", customStrSize, strlen(strBuffer));
    sendJson(strBuffer, 200);
    free(strBuffer); 

    uint32_t freeMem = system_get_free_heap_size();
    Serial.printf("%s After /api/list Free heap mem: %d\n", NTP.getTimeDateString().c_str(), freeMem);   
  });
  /**
   * This endpoint allows agent modules to register themselves to master when they initialize
   */
  server->on("/api/register", HTTP_POST, [](){
    char *jsonString;
    Serial.println("Registering module");
    // This will allocate jsonString
    XUtils::stringToCharP(server->arg("plain"), &jsonString);
    // agentCollection->add method need to copy the data since jsonString will be freed.
    Serial.println(jsonString); 
    Agent* agent = agentCollection->add(jsonString);
    free(jsonString);
    if(agent == NULL) {
      sendJson("{}", 500);
     _oledDisplay->setLine(1, "Registration failed", TRANSIENT, NOT_BLINKING);
    } else {
      sendJson("{}", 200);
      if(agent->getToRename()) {
        agentToRename = agent;
      }
    }
    char message[100];
    sprintf(message, "Registered modules: %d", agentCollection->getCount());
   _oledDisplay->setLine(2, message, NOT_TRANSIENT, NOT_BLINKING);    
  });

  /**
   * This endpoint allows removing a module
   */
  server->on("/api/register", HTTP_DELETE, [](){
    char *jsonString;
    Serial.println("Unregistering module");

  });

  // This endpoint is used by modules when they want to update data in the agent collection
  // (which is the data that the UI is polling)
  server->on("/api/refresh", HTTP_POST, [](){
    char *jsonString;
    Serial.println("Refreshing module");
    // This will allocate jsonString
    XUtils::stringToCharP(server->arg("plain"), &jsonString);
    // agentCollection->add method need to copy the data since jsonString will be freed.
    Serial.println(jsonString); 
    Agent* agent = agentCollection->refresh(jsonString);
    free(jsonString);
    if(agent == NULL) {
      sendJson("{}", 500);
     _oledDisplay->setLine(1, "Refreshing failed", TRANSIENT, NOT_BLINKING);
    } else {
      sendJson("{}", 200);
    }          
  });
  
  // TODO: remove this or make it better. Needed during dev
  // reset may be only possible by SMS from admin number ?
  server->on("/api/swarmReset",  HTTP_GET, [](){
    Serial.println("Rq on /swarmReset");
    agentCollection->reset();
    config->initFromDefault();
    config->saveToEeprom();
    sendJson("{}", 200);
    gsm.sendSMS(config->getAdminNumber(), "Reset done");  // 
    WiFi.mode(WIFI_AP);
    initSoftAP();  
  });

#endif  
  // Return this module's custom data if any
  // Almost like ping request except for heap size. Is it worth it ? Could be exact same... 
  _server->on("/api/data", HTTP_GET, [&]() {
    sendData(true);
  });
  
  // The BackBone framework uses PUT to save data from UI to modules  
  _server->on("/api/data", HTTP_PUT, [&]() {
    _processPostPut();
  });
  // But the modules can't PUT, they POST: handle both
  _server->on("/api/data", HTTP_POST, [&]() {
    _processPostPut();
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
  
  // OTA: update 
  _server->on("/api/ota", HTTP_POST, [&]() {
    String forwardTo = _server->header("Xiot-forward-to");
    String jsonBody = _server->arg("plain");
    int httpCode = 200;
    if(forwardTo.length() != 0) {
      #ifdef XIOT_MASTER    
      Serial.print("Forwarding ota to ");
      Serial.println(forwardTo);
      char message[SSID_MAX_LENGTH + PWD_MAX_LENGTH + 40];
      sprintf(message, "{\"%s\":\"%s\",\"%s\":\"%s\"}", XIOTModuleJsonTag::ssid, config->getHomeSsid(), XIOTModuleJsonTag::pwd, config->getHomePwd());
      APIPost(forwardTo, "/api/ota", message, &httpCode, NULL, 0);
      #endif
      // TODO warn if not a master
    } else {        
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
      // OTA on master does not send the home ssid, master has it in config
      #ifdef XIOT_MASTER
      if(ssidp )= NULL)
        ssidp = config->getHomeSsid();
      if(pwdp == NULL)
        pwdp = config->getHomePwd();
      #endif
  
      httpCode = startOTA(ssidp, pwdp);
    }
    sendJson("{}", httpCode);      
  });
    
  _server->begin();
}    

// This is responding to api/ping and api/data (for symmetry with put/post on api/data)
// This is also when refreshing data
int XIOTModule::sendData(bool isResponse) {
  int httpCode = 200;
  const int bufferSize = JSON_OBJECT_SIZE(3);
  StaticJsonBuffer<bufferSize> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  char macAddrStr[80]; // musn't be declared in the if !response block: would be "freed" too soon
  
  // Return heap size for health monitoring
  uint32_t freeMem = system_get_free_heap_size();
  Serial.printf("Free heap mem: %d\n", freeMem);  
  root[XIOTModuleJsonTag::heap] = freeMem ;
  
  char *customData = _customData();
  int payloadSize = 100; // for all curly brackets, comas, quotes, ... (2 or 3 attributes max)
  if(customData) {
    root[XIOTModuleJsonTag::custom] = customData ;
    payloadSize += strlen(customData);
  }
  // If this is a refresh request (not a ping response), we need to include the MAC address
  if(!isResponse) {
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    sprintf(macAddrStr, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
    root[XIOTModuleJsonTag::MAC] = macAddrStr ;
    payloadSize += strlen(macAddrStr);
  }
  char* payloadStr = (char *) malloc(payloadSize);
  root.printTo(payloadStr, payloadSize);
  if(isResponse) {
    sendJson(payloadStr, httpCode);
  } else {
    masterAPIPost("/api/refresh", String(payloadStr), &httpCode, NULL, 0);
  }
  free(customData);
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
    // We'll refresh the data on master once this request callback is done
    _refreshNeeded = true;      
  }
  sendJson(response, httpCode);
  free(response);        
  free(bodyStr);   
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
  char message[JSON_STRING_CONFIG_SIZE];
  char macAddrStr[100];
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(macAddrStr, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0],macAddr[1],macAddr[2],macAddr[3],macAddr[4],macAddr[5]);
  _oledDisplay->setLine(1, "Registering", TRANSIENT, NOT_BLINKING);
  _wifiDisplay();
  StaticJsonBuffer<JSON_BUFFER_CONFIG_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root[XIOTModuleJsonTag::name] = _config->getName();
  root[XIOTModuleJsonTag::ip] = _localIP;
  root[XIOTModuleJsonTag::MAC] = macAddrStr;
  root[XIOTModuleJsonTag::uiClassName] = _config->getUiClassName();
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
  root.printTo(message, JSON_STRING_CONFIG_SIZE);
  free(customPayload);

  //Serial.println(message);
  masterAPIPost("/api/register", message, &httpCode);
  if(httpCode == 200) {
    _canRegister = false;
    _oledDisplay->setLine(1, "Registered", TRANSIENT, NOT_BLINKING);
    customRegistered(true);
  } else {
    _oledDisplay->setLine(1, "Registration failed", TRANSIENT, NOT_BLINKING);
    customRegistered(false);
  }
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

bool XIOTModule::isOTAStarted() {
  return _otaIsStarted;
}

// Set the module in Update Waiting Mode: connect to ssid if provided, and setup the stuff
void XIOTModule::_setupOTA() {
  ArduinoOTA.onStart([&]() {
    _oledDisplay->setLine(1, "Loading...", NOT_TRANSIENT, BLINKING);
    _oledDisplay->setLine(2, "Start updating", TRANSIENT, NOT_BLINKING);
  });  
  ArduinoOTA.onEnd([&]() {
    _oledDisplay->setLine(2, "End updating", TRANSIENT, NOT_BLINKING);
  });  
  ArduinoOTA.onProgress([&](unsigned int progress, unsigned int total) {
    char message[50];
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
  _oledDisplay->setLine(1, "Waiting for OTA", NOT_TRANSIENT, BLINKING);
  _oledDisplay->setLine(2, "", NOT_TRANSIENT, NOT_BLINKING);
  _otaIsStarted = true;
  if(ssid != NULL && strlen(ssid) > 0) {
    Serial.printf("Connecting to %s\n", ssid);
    WiFi.begin(ssid, pwd);
  }
  ArduinoOTA.begin();
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
  if(_otaIsStarted) return;
  
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
  unsigned int timeNow = millis();
  
  now(); // Needed to update the clock from the TimeLib library
  // (and used by NTP library)
      
  // Check if any request to serve
  _server->handleClient();
    
  if(_otaIsStarted) {
    _oledDisplay->refresh();
    ArduinoOTA.handle();
    return;
  }
  
  // check if any new added agent needs to be renamed
  //  TODO: get rid of this ? handle it in agentCollection
  #ifdef XIOT_MASTER
  if(agentToRename != NULL) {
    agentCollection->autoRename(agentToRename);
    agentToRename = NULL;
  }
      
  if(timeNow - timeLastPing >= MIN_PING_PERIOD*1000) {
    timeLastPing = timeNow; 
    agentCollection->ping();
    uint32_t freeMem = system_get_free_heap_size();
    Serial.printf("%s After ping Free heap mem: %d\n", NTP.getTimeDateString().c_str(), freeMem);  
  }
  #endif
    
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

bool XIOTModule::customBeforeOTA() {
  // Override this method to implement custom processing before initiating OTA.
  // You can block it returning false.
  return true;
}




