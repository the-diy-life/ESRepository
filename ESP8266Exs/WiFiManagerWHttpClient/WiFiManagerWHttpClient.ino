
// Common libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

//WiFi Manager libraries
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//HttpClient libraries
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

// New definitions

char http_server[40];
char http_port[6] = "8080";
char username[34] = "admin";
char userpassword[34] = "admin";
char* APName = "AutoConnectAP";
char* APPassword = "password";
char* SSID;
char* password;

//flag for saving data
bool shouldSaveConfig = false;

// The definitions from WiFi Manager
/*
  //define your default values here, if there are different values in config.json, they are overwritten.
  char http_server[40];
  char http_port[6] = "8080";
  char blynk_token[34] = "YOUR_BLYNK_TOKEN";

  //flag for saving data
  bool shouldSaveConfig = false;

  // For first test
  char* APName = "AutoConnectAP";
  char* APPassword = "password";
  char* StaName = "Profit";
  char* StaPassword = "Pro.Profit";
*/

// The definitions from http client
/*
  #ifndef STASSID
  #define STASSID "Focus"
  #define STAPSK  "Focus@Pro"
  #endif

  const char* ssid = STASSID;
  const char* password = STAPSK;
*/
ESP8266WebServer server(90);
ESP8266WiFiMulti WiFiMulti;



// Methods from WiFi Manager
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Methods from Http Client
//Check if header is present and correct
bool is_authenticated() {
  Serial.println("Enter is_authenticated");
  if (server.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      Serial.println("Authentication Successful");
      return true;
    }
  }
  Serial.println("Authentication Failed");
  return false;
}

//login page, also called for disconnect
void handleLogin() {
  String msg;
  if (server.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
  }
  if (server.hasArg("DISCONNECT")) {
    Serial.println("Disconnection");
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
    server.send(301);
    return;
  }
  String usernameStr(username);
  String userpasswordStr(userpassword);

  if (server.hasArg("USERNAME") && server.hasArg("PASSWORD")) {

    if (server.arg("USERNAME") == usernameStr &&  server.arg("PASSWORD") == userpasswordStr) {
      server.sendHeader("Location", "/");
      server.sendHeader("Cache-Control", "no-cache");
      server.sendHeader("Set-Cookie", "ESPSESSIONID=1");
      server.send(301);
      Serial.println("Log in Successful");
      return;
    }
    msg = "Wrong username/password! try again.";
    Serial.println("Log in Failed");
  }
  String content = String("<html><body><form action='/login' method='POST'>To log in, please use : ") + usernameStr + "/" + userpasswordStr + "<br>";
  content += "User:<input type='text' name='USERNAME' placeholder='user name'><br>";
  content += "Password:<input type='password' name='PASSWORD' placeholder='password'><br>";
  content += "<input type='submit' name='SUBMIT' value='Submit'></form>" + msg + "<br>";
  content += "You also can go <a href='/inline'>here</a></body></html>";
  server.send(200, "text/html", content);
}

//root page can be accessed only if authentication is ok
void handleRoot() {
  Serial.println("Enter handleRoot");
  String header;
  if (!is_authenticated()) {
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(301);
    return;
  }
  String content = "<html><body><H2>hello, you successfully connected to esp8266!</H2><br>";
  if (server.hasHeader("User-Agent")) {
    content += "the user agent used is : " + server.header("User-Agent") + "<br><br>";
  }
  content += "You can access this page until you <a href=\"/login?DISCONNECT=YES\">disconnect</a></body></html>";

  server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
  server.send(200, "text/html", content);
}

//no need authentication
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void ReadConfig(){
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
          /*
            char username[34] = "admin";
            char userpassword[34] = "admin";*/
          strcpy(http_server, json["http_server"]);
          strcpy(http_port, json["http_port"]);
          strcpy(username, json["username"]);
          strcpy(userpassword, json["userpassword"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // WiFi Manager setup code ****************************************** Start****************************
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    ReadConfig();
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_http_server("server", "http server", http_server, 40);
  WiFiManagerParameter custom_http_port("port", "http port", http_port, 6);
  WiFiManagerParameter custom_username("username", "username", username, 32);
  WiFiManagerParameter custom_userpassword("userpassword", "userpassword", userpassword, 32);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_http_server);
  wifiManager.addParameter(&custom_http_port);
  wifiManager.addParameter(&custom_username);
  wifiManager.addParameter(&custom_userpassword);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(APName, APPassword)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  //read updated parameters
  strcpy(http_server, custom_http_server.getValue());
  strcpy(http_port, custom_http_port.getValue());
  strcpy(username, custom_username.getValue());
  strcpy(userpassword, custom_userpassword.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["http_server"] = http_server;
    json["http_port"] = http_port;
    json["username"] = username;
    json["userpassword"] = userpassword;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    ReadConfig();
    
  // Http client code ########################################## Start ######################################

  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works without need of authentication");
  });

  server.onNotFound(handleNotFound);
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent", "Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  int port = atoi(http_port);
  Serial.println("Port No." + port);
  Serial.println(port , DEC);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize);
  if (port) {
    server.begin(port);
    Serial.println("HTTP server started");
  }
  else {
    Serial.println("invalid port number...");

  }


  // Http client code ########################################## End ######################################


  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  // WiFi Manager setup code ****************************************** End ****************************
}

void loop() {

  server.handleClient();

}
