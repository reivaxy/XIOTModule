

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
 * This constructor is used by slave modules that take full advantage of this class
 */
XIOTModule::XIOTModule(ModuleConfigClass* config, int displayAddr, int displaySda, int displayScl) {
  _config = config;
  Serial.print("Initializing module ");
  Serial.println(config->getName());
    
  // Initialise the OLED display
  _initDisplay(displayAddr, displaySda, displayScl);
  
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
  JsonObject& root = masterAPIGet("/api/config", &httpCode);
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
  char message[101];
  // TODO: Use dynamic buffer ?
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject(); 
  root[XIOTModuleJsonTag::name] = _config->getName();
  root[XIOTModuleJsonTag::slaveIP] = _localIP;
  root.printTo(message, 100);
  masterAPIPost("/api/register", message, &httpCode);
  if(httpCode == 200) {
    _canRegister = false;
  } else {
    Serial.println("Registration failed");
  } 
}


/**
 * Send a GET request to master 
 * Returns received json
 */
JsonObject& XIOTModule::masterAPIGet(const char* path, int* httpCode) {
  Debug("XIOTModule::masterAPIGet\n");
  String masterIP = WiFi.gatewayIP().toString();
  return APIGet(masterIP, path, httpCode);
}

/**
 * Send a GET request to given IP 
 * Returns received json
 */
JsonObject& XIOTModule::APIGet(String ipAddr, const char* path, int* httpCode) {
  Debug("XIOTModule::APIGet\n");
  HTTPClient http;
  http.begin(ipAddr, 80, path);
  *httpCode = http.GET();
  if(*httpCode <= 0) {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(*httpCode).c_str());
    return JsonObject::invalid();
  }
  String jsonResultStr = http.getString();
  http.end();
  Serial.println(jsonResultStr);
  StaticJsonBuffer<1000> jsonBuffer; 
  JsonObject& root = jsonBuffer.parseObject(jsonResultStr);
  if(!root.success()) {
    Serial.println("XIOTModule::APIGet Json parsing failure");
  }
  return root;
}

/**
 * Send a POST request to master 
 * Returns received json
 */
JsonObject& XIOTModule::masterAPIPost(const char* path, String payload, int* httpCode) {
  Debug("XIOTModule::masterAPIPost\n");
  String masterIP = WiFi.gatewayIP().toString();
  return APIPost(masterIP, path, payload, httpCode);
}

/**
 * Send a POST request to given IP 
 * Returns received json
 */
JsonObject& XIOTModule::APIPost(String ipAddr, const char* path, String payload, int* httpCode) {
  Debug("XIOTModule::APIPost\n");
  HTTPClient http;
  http.begin(ipAddr, 80, path);
  *httpCode = http.POST(payload);
  if(*httpCode <= 0) {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(*httpCode).c_str());
    return JsonObject::invalid();
  }
  String jsonResultStr = http.getString();
  http.end();
  Serial.println(jsonResultStr);
  // TODO: use a dynamic buffer ? 
  StaticJsonBuffer<1000> jsonBuffer; 
  JsonObject& root = jsonBuffer.parseObject(jsonResultStr);
  if(!root.success()) {
    Serial.println("XIOTModule::APIPost Json parsing failure");
  }  
  return root;
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






