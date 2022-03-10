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
</style>\
</head>\
<body>\
<h1>" MSG_INIT_WELCOME " %s</h1>\
<form action='/api/saveConfig' method='post'>\
  <input name='autonomous' type='checkbox' %s/>" MSG_INIT_AUTONOMOUS "</br>\
  <input name='xiotSsid' type='text' placeholder='" MSG_INIT_AP_SSID "'/><br/>\
  <input name='xiotPwd' type='text' placeholder='" MSG_INIT_AP_PWD "'/><br/>\
  <input name='boxSsid' type='text' placeholder='" MSG_INIT_HOME_SSID "'/><br/>\
  <input name='boxPwd' type='text' placeholder='" MSG_INIT_HOME_PWD "'/><br/>\ 
  <!--input name='ntpHost' type='text' placeholder='" MSG_INIT_NTP_HOST "'/><br/>\
  <input name='webSite' type='text' placeholder='" MSG_INIT_WEBSITE "'/><br/>\
  <input name='apiKey' type='text' placeholder='" MSG_INIT_API_KEY "'/><br/-->\
  %s\
  <input type='submit'/>\
</form>\
%s\
</body>\
</html>\
";