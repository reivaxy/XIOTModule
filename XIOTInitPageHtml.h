/**
 *  iotinator XIOT module init page 
 *  Xavier Grosjean 2021
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International Public License
 */
 #pragma once

 #include "XIOTMessages.h"

char moduleInitPage[] = "\
<html title='Agent init page'>\
<head>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<style>\
body{line-height:1.5em}\
input{left:200px;position:absolute}\
a{right:40px;position:absolute;text-decoration:none}\
</style>\
</head>\
<body>\
<h1> %s " MSG_INIT_WELCOME "</h1>\
<form action='/api/saveConfig' method='post'>\
  " MSG_INIT_NAME " <input name='name' type='text' value='%s'/></br>\
  " MSG_INIT_AUTONOMOUS " <input name='autonomous' type='checkbox' %s/></br>\
  " MSG_INIT_AP_SSID " <input name='xiotSsid' type='text'/><br/>\
  " MSG_INIT_AP_PWD " <input name='xiotPwd' type='text'/><br/>\
  " MSG_INIT_HOME_SSID " <input name='boxSsid' type='text'/><br/>\
  " MSG_INIT_HOME_PWD " <input name='boxPwd' type='text'><br/>\ 
  " MSG_INIT_TIME_OFFSET " <input name='timeOffset' type='number' value='%d'><br/>\ 
  " MSG_INIT_PO_USER " <input name='poUser' type='text'/><br/>\
  " MSG_INIT_PO_TOKEN " <input name='poToken' type='text'/><br/>\
  <!--" MSG_INIT_NTP_HOST " <input name='ntpHost' type='text'/><br/>\
  " MSG_INIT_WEBSITE " <input name='webSite' type='text'/><br/>\
  " MSG_INIT_API_KEY " <input name='apiKey' type='text'/><br/-->\
  %s\
  <input type='submit'/>\
</form>\
<br/><br/><br/><form action='/api/ota' method=POST><input type='submit' value='Upgrade Firmware'/></form>\
%s\
</body>\
</html>\
";