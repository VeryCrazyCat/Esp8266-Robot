/*
 * WebSocketClientSocketIO.ino
 *
 *  Created on: 06.06.2016
 *
 */

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include <Hash.h>

#include <Servo.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <EasyButton.h>

ESP8266WiFiMulti WiFiMulti;

#define LED 2
#define BUTTON_PIN D0

EasyButton button(BUTTON_PIN);

bool connectingToRemote = false;

bool endServer = false;

Servo ESCLeft;
int ESCLeft_value;

Servo ESCRight;
int ESCRight_value;

//Your Domain name with URL path or IP address with path
String serverName = "http://192.168.0.217:5000/";

unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

int mode;

WiFiClient client;
HTTPClient http;

String remote_ssid     = "ESP8266-Access-Point";
String remote_password = "123456789";

const char* ssid     = "ESP8266-Access-Point";
const char* password = "123456789";
String webPage,wifi_name, wifi_password;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


// SET UP WIFI WEB INTERFACE

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
    .input {
      font-size: 1rem;
      width: 400px;
      height: 50px;
    }
  </style>
</head>
<body>
  <h2>STEM Fair 2025</h2>
  <p class="state"></p>
  <p><button id="toggle_button" class="button">Toggle</button></p>
  <p><button id="toggle_motor1" class="button">Toggle Left Motor</button></p>
  <p><button id="toggle_motor2" class="button">Toggle Right Motor</button></p>
  <p>_________________________</p>
  <h2>Connect to Wifi network</h2>
  <p class="display"></p>
  <form method="POST"action="/postForm">
  <p>Enter network name here:</p>
  <input type="text" name="wifi-name" value="VM7893174">
  <p>Enter network password here:</p>
  <input type="text" name="wifi-password" value="svgMrsTeacgsi8yt">
  <input type="submit" value="Post Credentials">
  </form>

  <p>_____________</p>

  <form method="POST"action="/switchToRemote">
  <input type="submit" value="Switch to remote mode">
  </form>
  <script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
  }
  function onOpen(event) {
    console.log('Connection opened');
  }

  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    document.querySelector('.state').innerHTML = event.data;
  }
  window.addEventListener('load', onLoad);
  function onLoad(event) {
    initWebSocket();
    initButton();
  }
  function initButton() {
    document.getElementById('toggle_button').addEventListener('click', toggleLED);
    document.getElementById('toggle_motor1').addEventListener('click', toggleMotor1);
    document.getElementById('toggle_motor2').addEventListener('click', toggleMotor2);
  }
  function toggleLED(){
    websocket.send('toggle');
  }
  function toggleMotor1(){
    websocket.send('motor1');
  }
  function toggleMotor2(){
    websocket.send('motor2');
  }
  </script> 
</body>
</html>)rawliteral";



//FLASH LED 3 TIMES

void flashLED(int number) {
  for(int i=0; i<number; i++){
    digitalWrite(LED, HIGH);
    delay(250);
    digitalWrite(LED, LOW);
    delay(250);
  }
}

//SET UP WIFI CONNECTION INTERFACE

void switchModes() {
  if (mode == 0) {
    mode = 1;
    setupremote();
  }
  if (mode==1) {
    mode= 0;
    setup();
  }
}


//https://randomnerdtutorials.com/esp8266-nodemcu-websocket-server-arduino/.

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "toggle") == 0) {
      digitalWrite(LED, !digitalRead(LED));
      //notifyClients();
    }
    if (strcmp((char*)data, "motor1") == 0) {
      if (ESCLeft_value == 40) {
        ESCLeft_value = 0;
      }
      else{
        ESCLeft_value = 40;
      }
      //notifyClients();
    }
    if (strcmp((char*)data, "motor2") == 0) {
      if (ESCRight_value == 40) {
        ESCRight_value = 0;
      }
      else{
        ESCRight_value = 40;
      }
      //notifyClients();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


void setup() {
  ESCLeft.attach(D3);
  ESCRight.attach(D4);
  ESCLeft_value = 0;
  ESCRight_value = 0;
  Serial.begin(115200);

  button.begin();
  button.onPressed(switchModes);

  WiFi.mode(WIFI_AP);
  mode = 0;
  

  ESCLeft_value = 0;
  ESCRight_value = 0;
  
  // put your setup code here, to run once:
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/postForm", HTTP_POST, [](AsyncWebServerRequest *request) {
    remote_ssid = request->arg("wifi-name");
    remote_password = request->arg("wifi-password");
    
    Serial.println("\nCredentials Received:");
    Serial.print("SSID: ");
    Serial.println(remote_ssid);
    Serial.print("Password: ");
    Serial.println(remote_password);
    
    flashLED(2);
    request->send_P(200, "text/html", index_html);
  });

  server.on("/switchToRemote", HTTP_POST, [](AsyncWebServerRequest *request) {
    
    flashLED(4);
    endServer = true;
  });
  initWebSocket();
  server.begin();
  pinMode(LED, OUTPUT);
}

void registerSetupRemote() {
  endServer = false;
  setupremote();

}


//SETUP REMOTE, CONNECTING TO SERVER AND AUTOMATIC DRIVING

void setupremote() {
  ws.closeAll();
  
  //server.end(); // Stop the web server
  //server.reset();
  WiFi.softAPdisconnect(true);
  Serial.println("Server ended");
  
  // USE_SERIAL.begin(921600);
  Serial.println("Disconnecting from network");
  WiFi.disconnect();
  while (WiFi.status() != WL_DISCONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.printf("Connecting to %s\n", remote_ssid);
  WiFi.begin(remote_ssid.c_str(), remote_password.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }
  String ip = WiFi.localIP().toString();

  Serial.printf("[SETUP] WiFi Connected %s\n", ip.c_str());
  
  
  delay(1);
  Serial.println("Connected to remote network!");
  // Client address
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  

  
  mode = 1;
    
}

//EMIT DATA BACK TO SERVER
void emit(String data1) {
  WiFiClient client;
  HTTPClient http;
  String serverPath = serverName;
  http.begin(client, serverPath.c_str());

  http.addHeader("Content-Type", "Content-Type: application/json"); 
  StaticJsonDocument<200> doc;
  doc["data"] = data1;

  String jsonPayload;
  serializeJson(doc, jsonPayload);
  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println(response);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// PROCESSES INSTRUCTION FORMAT SENT BY SERVER

void processActions(String actions) {
  int arr[99];
  int i,j,k=0;
  i = actions.indexOf('[');
    
  //splits string, convert 'string' values to integers and place into int array
  //https://forum.arduino.cc/t/convert-string-to-an-array/1133788/4
  while(i++>-1){
    j = actions.indexOf(',',i);
    arr[k++]= actions.substring(i,j).toInt();
    i = actions.indexOf(',',j);
  }
  
  for(i=0; i<k; ++i){
    if (arr[i] == 1) {
      Serial.println("Received signal");
      Serial.print(arr[i]);

      digitalWrite(LED, !digitalRead(LED));
      String stateStr = (!digitalRead(LED) == HIGH) ? "HIGH" : "LOW";
      Serial.println(stateStr);
      emit(stateStr);
    }
    if (arr[i] == 11) {
      Serial.println("Received signal");
      Serial.print(arr[i]);

      ESCLeft_value = 40;
    }
    if (arr[i] == 10) {
      Serial.println("Received signal");
      Serial.print(arr[i]);

      ESCLeft_value = 0;
    }

    if (arr[i] == 21) {
      Serial.println("Received signal");
      Serial.print(arr[i]);

      ESCRight_value = 40;
    }
    if (arr[i] == 20) {
      Serial.println("Received signal");
      Serial.print(arr[i]);

      ESCRight_value = 0;
    }
  }
}
void loop() {
  //button.read();
  ws.cleanupClients();
  ESCLeft.write(ESCLeft_value);
  ESCRight.write(ESCRight_value);
  if (endServer == true) {
    Serial.println("Received end server signal");
    registerSetupRemote();
  }
  if (mode == 1) {
    
    SSE(); // Only run SSE after connection is established
  };
  
 // int httpCode = http.GET();
  

  yield();
}


// ESTABLISHES SSE CONNECTION BETWEEN THE SERVER AND ESP8266
void SSE() {
  bool execute = true;

  if (!client.connected()) {
    connectToServer();
    execute = false;
  }
  

  //If all conditions are OK, then loop begins execution
  if (execute) {
    //Serial.println("Connected to SSE stream");

    while (client.available()) {  // Keep reading as long as connected
      //PROBES FOR REQUESTS FROM SERVER
      String line = client.readStringUntil('\n'); // Read until newline
      line.trim(); // Remove leading/trailing whitespace
      //Serial.println(line);
      // Parse SSE data (lines starting with "data:")
      if (line.startsWith("data:")) {
        String payload = line.substring(5); // Extract data after "data:"
        payload.trim();
        //Serial.print("Received: ");
        //Serial.println(payload);
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print("JSON Parsing Failed: ");
          Serial.println(error.c_str());
          return;
        }

        // Extract values from JSON
        double time = doc["value"]; // Replace "value" with your JSON key
        //Serial.print("Received time: ");
        //Serial.println(time, 6); // Print with 6 decimal places

        String actions = doc["actions"];
        processActions(actions);
      }

      
      delay(1); // Small delay to avoid overwhelming the serial port
    }

    //Serial.println("SSE stream disconnected");
  } 

  http.end(); // Close the connection
}

void connectToServer() {
  //Serial.printf("Connecting to %s:%d...\n", "192.168.0.217", 5000);
  
  if (!client.connect("192.168.0.217", 5000)) {
    Serial.println("Connection failed!");
    return;
  }

  //Serial.println("Connected to server!");
  
  // Send SSE HTTP request
  client.print("GET /sse HTTP/1.1\r\n");
  client.printf("Host: %s\r\n", "192.168.0.217");
  client.print("Accept: text/event-stream\r\n");
  client.print("Connection: keep-alive\r\n\r\n");
}
