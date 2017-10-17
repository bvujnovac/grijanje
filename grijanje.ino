#include "Timer.h"
#include "RestClient.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <SimpleDHT.h>
// Port defaults to 8266
Timer t;
Timer t1;
Timer wdt;

String response;
String data = "";

char val[] = "Metric";

int pinDHT22 = 2;
SimpleDHT22 dht22;

int hour = -1;
int minute = -1;
int seconds = -1;

float temperature = 0;
float humidity = 0;
int tempInt;


const int led = 13;

unsigned int localPort = 2390;      // local port to listen for UDP packets

RestClient client = RestClient("apidev.accuweather.com");

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
////////////////////////////////////////////////////////////////////////////////////
ESP8266WebServer server ( 80 );

void handleRoot() {
  digitalWrite ( led, 1 );
  char temp[600];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  int temperatureInt = temperature;
  int humidityInt = humidity;

  snprintf ( temp, 600,

             "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 grijanje</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Vanjska temperatura: %d C</p>\
    <p>Unutrasnja temperatura: %d C</p>\
    <p>Relativna vlaznost: %d % </p>\
  </body>\
</html>",

             hr, min % 60, sec % 60, tempInt, temperatureInt, humidityInt
           );
  server.send ( 200, "text/html", temp );
  digitalWrite ( led, 0 );
}


void handleNotFound() {
  digitalWrite ( led, 1 );
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  digitalWrite ( led, 0 );
}
//////////////////////////////////////////
void watchdogFeed() {
  ESP.wdtFeed();
  
}
///////////////////////////////////////Setup
void setup() {
  ESP.wdtDisable();
  Serial.begin(115200);
  client.begin("BiT", "mobitel16");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  ArduinoOTA.setHostname("grijanje");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  udp.begin(localPort);
  t.every(15000, execute);
  t1.every(15000, vrijeme);
  wdt.every(1000, watchdogFeed);

  server.on ( "/", handleRoot );
  server.onNotFound ( handleNotFound );
  server.begin();
}
////////////////////////////////////////////////////////////////////////

int find_text1(String needle, String haystack) {
  int foundpos = -1;
  for (int i = 0; i <= haystack.length() - needle.length(); i++) {
    if (haystack.substring(i, needle.length() + i) == needle) {
      foundpos = i;
    }
  }
  return foundpos;
}

///////////////////////////////////////////////////////////////////////////////
// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
////////////////////////////////////////////////////////////////////////////
void vrijeme() {
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
    hour = -1;
    minute = -1;
    seconds = -1;
  }
  else {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;


    int UTC2 = 2;
    // UTC is the time at Greenwich Meridian (GMT)
    hour = ((epoch  % 86400L) / 3600);
    if (hour == 22) {
      hour = 0;
    }
    else if (hour == 23) {
      hour = 1;
    }
    else {
      hour += UTC2;
    }
    minute = ((epoch % 3600) / 60);
    seconds = epoch % 60;
    /**
    Serial.print("Trenutno vrijeme: ");
    Serial.print(hour);
    Serial.print(":");
    Serial.print(minute);
    Serial.print(":");
    Serial.println(seconds);
*/
  }
}
/////////////////////////////////////////////////////////////////////////
void execute() {
  response = "";
  int statusCode1 = client.get("/currentconditions/v1/119909.json?language=en&apikey=hoArfRosT1215", &response);
  int temp = find_text1(val, response);
  String tempc = response.substring(temp);
  String tempValue = tempc.substring(17);
  tempInt = tempValue.toInt();

  dht22.read2(pinDHT22, &temperature, &humidity, NULL);

  //part for outside temeperature
  if (tempInt >= 20) {
    //do something
  }
  else if (tempInt >= 16 && tempInt <= 19) {
    //do something
  }
  else if (tempInt >= 10 && tempInt <= 15) {
    //do something
  }
  else if (tempInt >= 5 && tempInt <= 9) {
    //do something
  }
  else if (tempInt >= 0 && tempInt <= 4) {
    //do something
  }
  else if (tempInt >= -5 && tempInt <= -1) {
    //do something
  }
  else if (tempInt >= -10 && tempInt <= -4) {
    //do something
  }
  else if (tempInt <= -11) {
    //do something
  }
/**
  Serial.print("Vanjska temperatura: ");
  Serial.println(tempInt);
  Serial.print("Unutrasnja temperatura: ");
  Serial.println(temperature);
  Serial.print("Relativna vlaznost: ");
  Serial.println(humidity);
  response.remove(0);
  */
}

////////////////////////////////////////////////////////////////////////////
void loop() {
  ArduinoOTA.handle();
  t.update();
  t1.update();
  wdt.update();
  server.handleClient();

}
