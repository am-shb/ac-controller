#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>           // https://github.com/esp8266/Arduino
#include <md5.h>
#include <stdlib.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>           // https://github.com/tzapu/WiFiManager

#include <DoubleResetDetect.h>     // https://github.com/jenscski/DoubleResetDetect

#include <ESP8266httpUpdate.h>     // http://arduino.esp8266.com/Arduino/versions/2.0.0/doc/ota_updates/ota_updates.html#http-server

#include <PubSubClient.h>          // https://github.com/knolleary/pubsubclient

#include "Thermostat.h"

#define VERSION_NUMBER 1
#define MQTT_INTERVAL 5000

DoubleResetDetect drd(2.0, 0x00);

WiFiClient espClient;
WiFiManager wifiManager;
PubSubClient mqttClient(espClient);

Thermostat thermostat;

long lastMsg = 0;
char msg[50];
int value = 0;
String DEVICE_ID = "00000000000000000000000000000000";

char token[40] = "";

//flag for saving data
bool shouldSaveConfig = false;

bool validate_token() {
  if(token[0] == 0)
    return false;
    
  return true;
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(115200);
  
  Serial.println("Booting");

   //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
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

          strcpy(token, json["token"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  if(!validate_token()) {
    Serial.println("Invalid token, Resetting...");
    wifiManager.resetSettings();
  }

  thermostat.setup();

  MD5Builder md5;
  md5.begin();
  md5.add(WiFi.macAddress());
  md5.calculate();
  DEVICE_ID = md5.toString();
  Serial.print("Device ID: ");
  Serial.println(DEVICE_ID);

  if (drd.detect())
  {
      Serial.println("** Double reset boot **");
      wifiManager.resetSettings();
  }
  else
  {
      Serial.println("** Normal boot **");
  }

  WiFiManagerParameter wifi_token("t", "t", token, 40);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&wifi_token);
  wifiManager.autoConnect("COOLER");
  //if you get here you have connected to the WiFi
  Serial.println("WiFi Connection Established.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  strcpy(token, wifi_token.getValue());
  Serial.print("Received token is: ");
  Serial.println(token);
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["token"] = token;

    File configFile = SPIFFS.open("/config.json", "w+");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("Checking for latest firmware");
  t_httpUpdate_return ret = ESPhttpUpdate.update("http://c70.ir/esp/update.php", String(VERSION_NUMBER));
  switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }

    mqttClient.setServer("c70.ir", 1883);
    mqttClient.setCallback(mqttMessageCallback);
}

void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    msg[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  msg[length] = '\0';
  Serial.println();

  String tpc = String(topic);
  if(tpc == "relays/water") {
    // thermostat.setAuto(false);
    thermostat.setRelayState("water", payload[0] == '1');
  }
  else if(tpc == "relays/motor") {
    // thermostat.setAuto(false);
    thermostat.setRelayState("motor", payload[0] == '1');
  }
  else if(tpc == "relays/fast") {
    // thermostat.setAuto(false);
    thermostat.setRelayState("fast", payload[0] == '1');
  }
  else if(tpc == "auto/status") {
    thermostat.setAuto(payload[0] == '1');
  }
  else if(tpc == "auto/temperature") {
    thermostat.setDesiredTemp(atof(msg));
  }
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += DEVICE_ID;
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe((String(token) + "/auto/status").c_str());
      mqttClient.subscribe((String(token) + "/auto/temperature").c_str());
      
      mqttClient.subscribe((String(token) + "/relays/water").c_str());
      mqttClient.subscribe((String(token) + "/relays/motor").c_str());
      mqttClient.subscribe((String(token) + "/relays/fast").c_str());

      Serial.println("SUBSCRIBE");
      Serial.println((String(token) + "/relays/fast").c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  thermostat.loop();

  long now = millis();
  if (now - lastMsg > MQTT_INTERVAL) {
    lastMsg = now;
    mqttClient.publish((String(token) + "/temperature").c_str(), String(thermostat.getCurrentTemp()).c_str(), true);
    mqttClient.publish((String(token) + "/relays/water").c_str(), thermostat.getRelayState("water") ? "1" : "0", true);
    mqttClient.publish((String(token) + "/relays/motor").c_str(), thermostat.getRelayState("motor") ? "1" : "0", true);
    mqttClient.publish((String(token) + "/relays/fast").c_str(), thermostat.getRelayState("fast") ? "1" : "0", true);
  }
}
