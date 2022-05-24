/**
 *  Debug stuff for iotinator master and agent modules 
 *  Xavier Grosjean 2021
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */

#ifdef DEBUG_XIOTMODULE
#define Debug(...) Serial.printf(__VA_ARGS__)
#else
#define Debug(...)
#endif

#ifdef DEBUG_XIOTMODULE_MEM
#define StackSize(msg) { if (XIOTModuleDebug::stackStart != NULL) {char here; Serial.printf("Stack Size %s at %s in %s (%d): %d\n", msg, __PRETTY_FUNCTION__,  __FILE__, __LINE__, XIOTModuleDebug::stackStart - &here);} }
#define HeapSize(msg) { Serial.printf("Free Heap %s at %s in %s (%d): %d (largest block %d)\n", msg,  __PRETTY_FUNCTION__,  __FILE__, __LINE__, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());} 
#define MemSize(msg) { StackSize(msg); HeapSize(msg);} 
#else
#define StackSize(...)
#define HeapSize(...)
#define MemSize(...)
#endif


#ifndef XIOTModuleDebugClassDefined
class XIOTModuleDebug {
public:
  static char* stackStart;
private:
   XIOTModuleDebug();
};
#endif
#define XIOTModuleDebugClassDefined