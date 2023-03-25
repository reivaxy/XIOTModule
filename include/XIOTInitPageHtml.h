/**
 *  iotinator XIOT module init page 
 *  Xavier Grosjean 2021
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */
 #pragma once

 #include "XIOTMessages.h"

#ifndef GIT_REV
  #define GIT_REVISION "unknown"
#else 
  #define GIT_REVISION GIT_REV
#endif

static const char moduleInitPage[] PROGMEM = "\
<html title='Agent init page'>\
<head>\
<meta charset='UTF-8'>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<style>\
body{line-height:1.5em}\
input{margin-bottom:10px;}\
a{right:40px;position:absolute;text-decoration:none}\
.red{background-color: red;font-weight: 900;}\
</style>\
</head>\
<body>\
<h3> %s " MSG_INIT_WELCOME "<a href='/'>🏠</a></h3>\
<form action='/api/saveConfig' method='post'>\
  " MSG_INIT_NAME " <br/><input name='name' type='text' value='%s' maxlength=20/></br>\
  " MSG_INIT_AUTONOMOUS "<input name='autonomous' type='checkbox' %s/></br>\
  " MSG_INIT_AP_SSID " <br/><input name='xiotSsid' type='text' maxlength=20/><br/>\
  " MSG_INIT_AP_PWD " <br/><input name='xiotPwd' type='text' maxlength=50/><br/>\
  " MSG_INIT_HOME_SSID " <br/><input name='boxSsid' type='text' maxlength=20/><br/>\
  " MSG_INIT_HOME_PWD " <br/><input name='boxPwd' type='text' maxlength=50><br/>\ 
  " MSG_INIT_BRIGHTNESS " <br/><input name='brightness' type='number' min=1 max=255 size=4/></br>\
  " MSG_INIT_TIME_OFFSET " <br/><input name='timeOffset' type='number' value='%d'><br/>\ 
  " MSG_INIT_FIREBASE_URL " <br/><input name='firebaseUrl' type='text' maxlength=150/><br/>\
  " MSG_INIT_FIREBASE_SECRET " <br/><input name='firebaseSecret' type='text' maxlength=49/><br/>\
  " MSG_INIT_PO_USER "<br/>" MSG_INFO_PO_TOKEN " <br/><input name='poUser' type='text' maxlength=40/><br/>\
  " MSG_INIT_PO_TOKEN " <br/><input name='poToken' type='text' maxlength=40/><br/>\
  " MSG_INIT_NOTIF_OFFLINE "<input name='ping' type='checkbox' %s/></br>\
  <!--" MSG_INIT_NTP_HOST " <br/><input name='ntpHost' type='text'/><br/>\
  " MSG_INIT_WEBSITE " <br/><input name='webSite' type='text'/><br/>\
  " MSG_INIT_API_KEY " <br/><input name='apiKey' type='text'/><br/-->\
  %s\
  <input type='submit'/>\
</form>\
<br/><br/>\
<form onsubmit=\"return confirm('" MSG_INIT_RESTART_CONFIRM "');\" action='/api/restart' method=GET><input class='red' type='submit' value='Restart'/></form>\
<br/><br/>\
<div>Version: " GIT_REVISION " </div>\
<form onsubmit=\"return confirm('" MSG_INIT_CONFIRM "');\" action='/api/ota' method=POST><input class='red' type='submit' value='Upgrade Firmware'/></form>\
%s\
</body>\
</html>\
";