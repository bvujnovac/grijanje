esp8266 heating controll in  my appartment

-this is in a very early stage, currently only works:

-getting outside temperature from accuweather.
-getting temperature radings from sensor in my room.
-OTA upgrade of eps8266.
-web server to show readings and values.
													  
To do: 

-fix C code to parse json response from server instead searching for a string inside a string and counting chars to needed value.
-make responsive web page template to incorporate buttons to controll heater manually (changing referent indoor temperature)
-make page template "look pretty"
-develop algorythm to decide how to turn reley ON or OFF based on temperatre values
-take in account next day temperature predictions (no sense in vasting energy for heating if its going to be warm)

