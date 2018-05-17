

#include "XIOTModule.h"

const char* XIOTModuleJsonTag::timestamp = "timestamp";
const char* XIOTModuleJsonTag::APInitialized = "APInitialized";
const char* XIOTModuleJsonTag::version = "version";
const char* XIOTModuleJsonTag::APSsid = "APSsid";
const char* XIOTModuleJsonTag::APPwd = "APPwd";
const char* XIOTModuleJsonTag::homeWifiConnected = "homeWifiConnected";
const char* XIOTModuleJsonTag::gsmEnabled = "gsmEnabled";
const char* XIOTModuleJsonTag::timeInitialized = "timeInitialized";

/**
 * This constructor is used by master iotinator, just to take advantage of
 * some methods available here.
 * Later, this class will be able to handle STA_AP
 */
XIOTModule::XIOTModule() {
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
    sendText("pong", 200);
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
    if(strcmp(pwd, _config->getPwd()) != 0) {
      _config->setSsid(ssid);
      _config->setPwd(pwd);
      _config->saveToEeprom();
      _connectSTA();
      
    }
  }    
}

void XIOTModule::_register() {
  int httpCode;
  masterAPIPost("/api/register", "{toto}", &httpCode);
  Serial.println(httpCode);
}

/**
 * Make a GET request to the master 
 * Returns received json
 */
JsonObject& XIOTModule::masterAPIGet(const char* path, int* httpCode) {
  Debug("XIOTModule::masterAPIGet\n");
  HTTPClient http;
  String masterIP = WiFi.gatewayIP().toString();
  http.begin(masterIP, 80, path);
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
  return root;
}

JsonObject& XIOTModule::masterAPIPost(const char* path, String payload, int* httpCode) {
  Debug("XIOTModule::masterAPIPost\n");
  HTTPClient http;
  String masterIP = WiFi.gatewayIP().toString();
  http.begin(masterIP, 80, path);
  *httpCode = http.POST(payload);
  if(*httpCode <= 0) {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(*httpCode).c_str());
    return JsonObject::invalid();
  }
  String jsonResultStr = http.getString();
  http.end();
  Serial.println(jsonResultStr);
  // TODO: 1000 ? make sure it's ok
  StaticJsonBuffer<1000> jsonBuffer; 
  JsonObject& root = jsonBuffer.parseObject(jsonResultStr);
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
  if(_wifiConnected && _canQueryMasterConfig) {
    _timeLastGet = timeNow;
    _canQueryMasterConfig = false;
    _getConfigFromMaster();
  }
  if(_wifiConnected && _canRegister) {
    _register(); 
    _canRegister = false;
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






