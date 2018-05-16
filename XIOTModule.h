/**
 *  Common stuff for iotinator master and slave modules 
 *  Xavier Grosjean 2018
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#define CONFIG_PAYLOAD_SIZE 600

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