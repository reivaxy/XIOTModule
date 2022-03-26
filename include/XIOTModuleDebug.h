/**
 *  Common stuff for iotinator master and agent modules 
 *  Xavier Grosjean 2021
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */


#ifdef DEBUG_XIOTMODULE
#define Debug(...) Serial.printf(__VA_ARGS__)
#else
#define Debug(...)
#endif
