#include <Arduino.h>
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <SoftwareSerial.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  
#include <FastLED.h>

#define HOST "api.twitch.tv"
#define PATH "/helix/streams?user_login="
#define POLL_SECONDS 10
#define LAMP_LEDS 18
#define LEDS_VIEWERS 12
#define SERIAL_DIGITS_DATA D7
#define SERIAL_DIGITS_PWR D8
#define NEOPIXEL_DATA D6


bool shouldSaveConfig = false;
SoftwareSerial digitsSerial(D2,SERIAL_DIGITS_DATA);
long last_viewer_count = 0;

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

char twitch_username[40] = "iMartyn";
char twitch_client_id[34] = "TWICH_CLIENT_ID";
CRGB leds[LAMP_LEDS+LEDS_VIEWERS];

void setup() {
  Serial.begin(115200);
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    // If you mess up your client id or similar, uncomment these lines for "factory defaults"
    /*
    SPIFFS.remove("/config.json");
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.eraseConfig();
    ESP.reset();
    ESP.restart();
    while (true) {
      yield();
    }
    */
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(twitch_username, json["twitch_username"]);
          strcpy(twitch_client_id, json["twitch_client_id"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_twitch_username("twitch_username", "twitch user", twitch_username, 40);
  WiFiManagerParameter custom_twitch_client_id("twitch_client_id", "twitch client id", twitch_client_id, 34);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  //add all your parameters here
  wifiManager.addParameter(&custom_twitch_username);
  wifiManager.addParameter(&custom_twitch_client_id);

  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  strcpy(twitch_username, custom_twitch_username.getValue());
  strcpy(twitch_client_id, custom_twitch_client_id.getValue());

  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["twitch_username"] = twitch_username;
    json["twitch_client_id"] = twitch_client_id;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  digitsSerial.begin(9600);
  FastLED.addLeds<NEOPIXEL, NEOPIXEL_DATA>(leds, LAMP_LEDS+LEDS_VIEWERS);
  pinMode(SERIAL_DIGITS_PWR, OUTPUT);
}

void loop() {
  Serial.print("connecting to ");
  Serial.println(HOST);
  
  // Use WiFiClient class to create TCP connections
  WiFiClientSecure client;
  if (!client.connect(HOST, 443)) {
    Serial.println("connection failed");
    return;
  } // We now create a URI for the request
  //DEBUG set to someone who's online
  //strcpy(twitch_username,"LocalDeluxe");
  Serial.print("Client id is: ");
  Serial.println(twitch_client_id);
  Serial.print("Requesting URL: ");
  Serial.print(PATH);
  Serial.println(twitch_username);
  
  // This will send the request to the server
  client.print(String("GET ") + PATH + String(twitch_username) + " HTTP/1.1\r\n" +
               "Client-ID: "+String(twitch_client_id)+"\r\n"
               "Host: " + HOST + "\r\n" + 
               "Connection: close\r\n\r\n");

  int32_t timeout = millis() + 1000;
  while (client.available() == 0) { 
    if (timeout - millis() < 0) { 
      Serial.println(">>> Client Timeout !"); 
      client.stop(); 
      return;
    }
  }
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  DynamicJsonBuffer jb(4096);
  String line = client.readStringUntil('\n');
  JsonObject& obj = jb.parseObject(line);
  boolean isStreaming = false;
  long viewer_count = 0;
  if (obj.success()) {
    JsonObject& subdoc = obj["data"][0];
    const char* liveness = subdoc["type"] | "offline";
    if (strcmp(liveness,"live") == 0) {
      isStreaming = true;
      viewer_count = subdoc["viewer_count"] | 0;
    }
  } else {
    Serial.println("Error parsing json.");
    Serial.println(line);
  }

  Serial.print("Streaming status: "); Serial.println(isStreaming);
  CRGB viewer_colour = CRGB::Green;
  long viewer_leds = viewer_count;
  if (viewer_leds > LEDS_VIEWERS) {
    viewer_colour = CRGB::Yellow;
    viewer_leds = viewer_leds / 10;
    if (viewer_leds > LEDS_VIEWERS) {
      viewer_colour = CRGB::Red;
      viewer_leds = viewer_leds / 10;
    }
  }
  Serial.print("Viewer count : ");
  Serial.println(viewer_count);
  if (viewer_count > 0) {
    digitalWrite(SERIAL_DIGITS_PWR, HIGH);
    delay(2000); //if I updated the firmware on the display... meh.
    if (viewer_count > 9999) {
      viewer_count = 9999;
    }
    char buf[4];
    sprintf(buf, "%04lu", viewer_count);
    digitsSerial.print(buf);
  } else {
    digitalWrite(SERIAL_DIGITS_PWR, LOW);
  }

  for (int i=0;i <LEDS_VIEWERS;i++) {
    leds[i] = CRGB::Black;
  }
  for (int i=LAMP_LEDS;i <viewer_leds+LAMP_LEDS;i++) {
    Serial.print("Setting LED ");
    Serial.println(i);
    leds[i] = viewer_colour.fadeLightBy( 168 );
  }
  if (isStreaming) {
    for (int i=0;i <LAMP_LEDS;i++) {
      leds[i] = CRGB::Red;
      leds[i] = leds[i].fadeLightBy( 64 ); // not quite so bright please!
    }
  } else {
    for (int i=0;i <LAMP_LEDS;i++) {
      leds[i] = CRGB::Black;
    }
  }
  FastLED.show();
  Serial.println("disconnecting from server.");
  client.stop();
  if ((last_viewer_count > 0) && (viewer_count > last_viewer_count)) {
    //quarter second flashes!
    for (int i=0; i<POLL_SECONDS; i++) {
      for (int i=0;i <LAMP_LEDS;i++) {
        leds[i] = CRGB::White;
        leds[i] = leds[i].fadeLightBy( 64 );
      }
      FastLED.show();
      delay(250);
      for (int i=0;i <LAMP_LEDS;i++) {
        leds[i] = CRGB::Red;
        leds[i] = leds[i].fadeLightBy( 64 ); 
      }
      FastLED.show();
      delay(750);
    }
  } else {
    delay(POLL_SECONDS*1000);
  }
  last_viewer_count = viewer_count;
}