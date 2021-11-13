#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <fauxmoESP.h>
#include "LittleFS.h"
#include <EasyButton.h>

AsyncWebServer server(80);

fauxmoESP fauxmo;

enum operationMode {
  askWifiCredentials,
  justPlug
};

void setupWifiAp(){
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("IntelliPlug1");
  Serial.println("AP address:" + WiFi.softAPIP().toString());

}

// Setup code necesary for setup mode
// We need to enable stuff to discover networks before creating a soft AP
// In which the user can give their network credential before normal usage
void wifiDeviceSetupSetup(){
  setupWifiAp();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/wifi_info.html", String(), false);
  });

  server.on("/available_networks", HTTP_GET, [](AsyncWebServerRequest *request){
    String returnObj = "{ \"wifiNetworks\": [";
    int networksFound = WiFi.scanNetworks();
    Serial.println("Networks found: " + networksFound);
    for(int i = 0; i < networksFound; i++){
      returnObj.concat("\"" + WiFi.SSID(i) + "\"");
      if(i != networksFound - 1){
        returnObj.concat(",");
      }
    }
    returnObj.concat("] }");
    Serial.println(returnObj);

    request->send(200, "application/json", returnObj);
  });

  server.on("/wifi_info", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid;
    String password;

    if(request->hasParam("ssid", true)){
      AsyncWebParameter* ssidParam = request->getParam("ssid", true);
      ssid = ssidParam->value();
    } else {
      request->send(400, "text/plain", "SSID parameter not found");
      return;
    }

    if(request->hasParam("password", true)){
      AsyncWebParameter* passwordParam = request->getParam("password", true);
      password = passwordParam->value();
    } else {
      request->send(400, "text/plain", "Password parameter not found");
      return;
    }

    Serial.println(("ssid:" + ssid + "\n" + "password:" + password).c_str());

    File f = LittleFS.open("/wifi.conf", "w");
    if(!f){
      request->send(500);
      return;
    }
    Serial.println("Saving connection parameters...");
    f.write(("ssid:" + ssid + "\n" + "password:" + password).c_str());
    f.close();
    request->send(200);
    ESP.restart();
  });
  server.begin();
  Serial.println("HTTP server started");
}

const char* enchufeUno = "Enchufe Uno";
const char* enchufeDos = "Enchufe Dos";

#ifdef ESP32
  #define RELAY_ENCHUFE_1 D2
  #define RELAY_ENCHUFE_2 D4
  #define TRIGGER_SETUP_MODE D13
  #define TOGGLE_RELAY_1 D18
  #define TOGGLE_RELAY_2 D19
#else
  #define RELAY_ENCHUFE_1 D1
  #define RELAY_ENCHUFE_2 D2
  #define TRIGGER_SETUP_MODE D5
  #define TOGGLE_RELAY_1 D6
  #define TOGGLE_RELAY_2 D7
#endif

EasyButton relayToggle1(TOGGLE_RELAY_1);
EasyButton relayToggle2(TOGGLE_RELAY_2);

void setupWifi(){
  //Encender wifi
  WiFi.mode(WIFI_STA);

  File file = LittleFS.open("/wifi.conf", "r");
  String wifiFileContents = file.readString();
  file.close();

  int newLineSeparator = wifiFileContents.indexOf('\n');
  String ssidFull = wifiFileContents.substring(0, newLineSeparator);
  String passwordFull = wifiFileContents.substring(newLineSeparator+1);

  String ssid = ssidFull.substring(5);
  String password = passwordFull.substring(9);
  Serial.println(("ssid:" + ssid + "\n" + "password:" + password).c_str());
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP Address: ");
  Serial.println(WiFi.localIP());
}

bool relayState1 = false;
bool relayState2 = false;

void normalOperationSetup(){
  Serial.println("IoT mode");
  
  pinMode(RELAY_ENCHUFE_1, OUTPUT);
  pinMode(RELAY_ENCHUFE_2, OUTPUT);
  digitalWrite(RELAY_ENCHUFE_1, LOW);
  digitalWrite(RELAY_ENCHUFE_2, LOW);
  relayToggle1.begin();
  relayToggle2.begin();

  setupWifi();

  // These two callbacks are required for gen1 and gen3 compatibility
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
      // Handle any other body request here...
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
      String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
      if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return;
      // Handle not found request here...
  });

  server.begin();

  fauxmo.createServer(false);
  fauxmo.setPort(80);
  fauxmo.enable(true);

  fauxmo.addDevice(enchufeUno);
  fauxmo.addDevice(enchufeDos);

  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
        if(strcmp(device_name,enchufeUno) == 0){
          relayState1 = state? HIGH: LOW;
          digitalWrite(RELAY_ENCHUFE_1, relayState1);
        } else if(strcmp(device_name, enchufeDos) == 0){
          relayState2 = state? HIGH: LOW;
          digitalWrite(RELAY_ENCHUFE_2, relayState2);
        }
    });
}

operationMode mode;

void setup() {
Serial.begin(115200);
Serial.setDebugOutput(true);
if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  if(LittleFS.exists("/wifi.conf")){
    mode = justPlug;
    normalOperationSetup();
  } else {
    mode = askWifiCredentials;
    wifiDeviceSetupSetup();
  }
}

void loop() {
  if(mode == justPlug){
    fauxmo.handle();

    if(relayToggle1.wasPressed()){
      relayState1 = !relayState1;
      fauxmo.setState(enchufeUno, relayState1, 255);
    }
    if(relayToggle2.wasPressed()){
      relayState2 = !relayState2;
      fauxmo.setState(enchufeDos, relayState2, 255);
    }
  }
}
