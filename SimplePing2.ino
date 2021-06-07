/*
 * With this library an ESP8266 can ping a remote machine and know if it's reachable. 
 * It provides some basic measurements on ping messages (avg response time).
 */

#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

const char* ssid     = "AA";
const char* password = "AA";
bool is9258Present;
bool TurnEmOn(bool i);
const IPAddress deskIP(192, 168, 0, 5);
const IPAddress powerIP(192,168,0,17); 
#define MAXTRIES 2
#define DOPOWEROFF true
int tryCount = MAXTRIES;

void setup() {
  Serial.begin(115200);
  delay(10);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println("Connecting to WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  pinMode(D4, OUTPUT);
  Serial.println();
  Serial.print("WiFi connected with ip ");  
  Serial.println(WiFi.localIP());

}

void loop() { 
    Serial.print("Pinging ip ");
  Serial.println(deskIP);
  if(Ping.ping(deskIP,1)) {
    Serial.println("Success!!");
    flash(1,100);
    tryCount = MAXTRIES;
    delay(1000);
    is9258Present = TurnEmOn(true);
  } else {
    Serial.println("Error :(");
    flash(2,100);
    delay(1000);
    if (tryCount-- <= 0) {
      is9258Present = TurnEmOn(false);
      tryCount = MAXTRIES;
    }
  }
  if (is9258Present)
    delay(9000);
  else
    flash(9,500);
   digitalWrite(D4, HIGH);
   delay(1000);
  }

void flash(int count, int period) {
  for (int i = 0; i < count; i++) {
    digitalWrite(D4, LOW);
    delay(period);
    digitalWrite(D4, HIGH);
    delay(period);
  }
}

  bool TurnEmOn(bool On) {
    bool ret = true;
    WiFiClient client;
    HTTPClient http;
    String cmdOff = "http://admin:12345678@"+powerIP.toString()+"/Set.cmd?cmd=setpower+p61=0+p62=0+p63=0+p64=0";
    String cmdOn = "http://admin:12345678@"+powerIP.toString()+"/Set.cmd?cmd=setpower+p61=1+p62=1+p63=1+p64=1";
    if (On == false && DOPOWEROFF == false)
      return true; // Don't turn off & pretend everything is sweet!
    Serial.print("[HTTP] begin...\n");
    // configure traged server and url
    if (Ping.ping(powerIP,1)) {
    http.begin(client, On ? cmdOn : cmdOff);
        Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
//        String payload = http.getString();
         int a = 0;
          byte ar[100];
          while ((ar[a] = http.getStream().read()) != 255 && a < 99) {
//            a = a < sizeof(ar) ? a++ : a;
          a++;
          }
          ar[a]=0;
          
          //String payload = http.getString();
          String payload = (char *)ar;
          // Serial.println(payload.length());
          Serial.println(payload);
              digitalWrite(D4, ar[10] == '1' && ar[16] == '1' && ar[22] == '1' && ar[28] == '1' ? LOW : HIGH);
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      ret = false;
    }
    http.end();
    } else
      ret = false;
    return ret;
  }
