/*
 * WebSocketClientSocketIO.ino
 *
 *  Created on: 06.06.2016
 *
 */



#include <iostream>
#include <cmath>

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


#include <Preferences.h>
Preferences prefs;

ESP8266WiFiMulti WiFiMulti;

#define LED 2
const int flashButtonPin = 0;


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
String server_domain = "192.168.0.217";
String server_port = "5000";

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
  <p>
  <input class="motor1input">
  <button id="toggle_motor1" class="button">Toggle Left Motor</button></p>
  <p>
  <input class="motor2input">
  <button id="toggle_motor2" class="button">Toggle Right Motor</button></p>
  <p>_________________________</p>
  <h2>Connect to Wifi network</h2>
  <p class="display"></p>
  <form method="POST"action="/postForm">
  <p>Enter network name here:</p>
  <input type="text" name="wifi-name" value="VM7893174">
  <p>Enter network password here:</p>
  <input type="text" name="wifi-password" value="svgMrsTeacgsi8yt">
  <p>Enter server domain here:</p>
  <input type="text" name="server-domain" value="192.168.0.217">
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
    const data = {
      LED: 0,
    };
    websocket.send(JSON.stringify(data));
  }
  function toggleMotor2() {
    let speed_ = document.querySelector(".motor2input").value
    const data = {
      motor: 2,
      speed: speed_,
    };
    websocket.send(JSON.stringify(data));
  }
  function toggleMotor1() {
    let speed_ = document.querySelector(".motor1input").value
    const data = {
      motor: 1,
      speed: speed_,
    };
    websocket.send(JSON.stringify(data));
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

//https://randomnerdtutorials.com/esp8266-nodemcu-websocket-server-arduino/.

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    StaticJsonDocument<512> doc;
    
    DeserializationError error = deserializeJson(doc, data);
    
    if (!error) {
      // Motor control commands
      if (doc.containsKey("motor")) {
        int motor = doc["motor"];
        int speed = doc["speed"];
        
        if (motor == 1) {
          ESCLeft_value = speed;
          Serial.printf("Left motor set to: %d\n", speed);
        } 
        else if (motor == 2) {
          ESCRight_value = speed;
          Serial.printf("Right motor set to: %d\n", speed);
        }
        
        // Immediate update
        ESCLeft.writeMicroseconds(ESCLeft_value);
        ESCRight.writeMicroseconds(ESCRight_value);
      }
      
      // LED toggle
      if (doc.containsKey("LED")) {
        digitalWrite(LED, !digitalRead(LED));
      }
    }
  }
}


//Follows Websocket protocol
void IRAM_ATTR onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
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
  flashLED(1);
  pinMode(LED, OUTPUT);
  ESCLeft.attach(D1);
  ESCRight.attach(D2);
  ESCLeft_value = 0;
  ESCRight_value = 0;
  ESCLeft.write(0);
  ESCRight.write(0);
  Serial.begin(115200);
  pinMode(flashButtonPin, INPUT_PULLUP);
  prefs.begin("stem-fair");

  if (prefs.getInt("mode", 0) == 0) {
    Serial.println("Setting up AP server");
    setupap();
  }
  if (prefs.getInt("mode", 0) == 1) {
    remote_ssid = prefs.getString("remote_ssid");
    remote_password = prefs.getString("remote_password");
    server_domain = prefs.getString("server_domain");
    Serial.println("Setting up remote on network: ");
    Serial.print(remote_ssid);
    setupremote();
  }

}
void setupap() {
  

  WiFi.mode(WIFI_AP);
  mode = 0;
  

  
  
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
    server_domain = request->arg("server-domain");
    
    Serial.println("\nCredentials Received:");
    Serial.print("SSID: ");
    Serial.println(remote_ssid);
    Serial.print("Password: ");
    Serial.println(remote_password);
    Serial.print("Server domain: ");
    Serial.println(server_domain);
    
    flashLED(2);
    request->send_P(200, "text/html", index_html);
  });

  server.on("/switchToRemote", HTTP_POST, [](AsyncWebServerRequest *request) {
    
    flashLED(4);
    registerSetupRemote();
  });
  initWebSocket();
  server.begin();
  
}

void registerSetupRemote() {
  prefs.putInt("mode", 1);
  prefs.putString("remote_ssid", remote_ssid);
  prefs.putString("remote_password", remote_password);
  prefs.putString("server_domain", server_domain);
  ESP.restart();

}

void registerSetupAP() {
  prefs.putInt("mode", 0);
  ESP.restart();

}



//SETUP REMOTE, CONNECTING TO SERVER AND AUTOMATIC DRIVING

void setupremote() {
  
  Serial.printf("Connecting to %s\n", remote_ssid);
  WiFi.begin(remote_ssid.c_str(), remote_password.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    ESCLeft.write(0);
    ESCRight.write(0);
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

  flashLED(3);
    
}

//EMIT DATA BACK TO SERVER
void emitData(String data1) {
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
    
    if (!arr[i] == 0) {
      Serial.println(ndigit(arr[i], 1));
      Serial.println("Received signal");
      Serial.print(arr[i]);
    }
    if (ndigit(arr[i], 1) == 1) {

      digitalWrite(LED, !digitalRead(LED));
      String stateStr = (!digitalRead(LED) == HIGH) ? "HIGH" : "LOW";
      Serial.println(stateStr);
      emitData(stateStr);
    }
    if (ndigit(arr[i], 1) == 2) { //1 -> motor
    
      //Example format: 1 1 1030
      //1 -> motor
      //1 -> right motor
      //1030 -> motor speed
      int motor_speed = arr[i] % 10000; // last 4 digits
      Serial.println("Motor speed: ");
      Serial.print(motor_speed);
      if (ndigit(arr[i], 2) == 0) {
        ESCLeft_value = motor_speed;
      }
      if (ndigit(arr[i], 2) == 1) {
        ESCRight_value = motor_speed;
      }
    }
  }
}
void loop() {
  //button.read();
  //if (mode == 0) {
   // ws.cleanupClients();
  //}
  
  ESCLeft.writeMicroseconds(ESCLeft_value);
  ESCRight.writeMicroseconds(ESCRight_value);
  int buttonState = digitalRead(flashButtonPin);

  if (buttonState == LOW) { // Flash button pressed
    Serial.println("Button pressed!");
    registerSetupAP();
    delay(200); 
  }
  if (mode == 1) {
    SSE();
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
  
  // if (!client.connect(server_domain, server_port)) {
  //   Serial.println("Connection failed!");
  //   return;
  // }
  
  //Serial.println("Connected to server!");
  char domain[40];
  server_domain.toCharArray(domain, 40);
  int port = server_port.toInt();
  // Send SSE HTTP request
  client.print("GET /sse HTTP/1.1\r\n");
  client.printf("Host: %s\r\n", domain);
  client.print("Accept: text/event-stream\r\n");
  client.print("Connection: keep-alive\r\n\r\n");
}


// Function to get the n-th digit of a number (DEEPSEEK)
int ndigit(int number, int position) {
    number = abs(number);
    
    // Handle invalid positions
    if(position <= 0) return -1;
    
    // Count total digits
    int digitCount = number == 0 ? 1 : 0;
    int temp = number;
    while(temp != 0) {
        digitCount++;
        temp /= 10;
    }
    
    // Position exceeds number length
    if(position > digitCount) return -1;
    
    // Calculate divisor using integer math
    int divisor = 1;
    for(int i = 1; i < digitCount - position + 1; i++) {
        divisor *= 10;
    }
    
    return (number / divisor) % 10;
}


int middlendigit(int number, int startPos, int n) {
    number = abs(number);
    // Calculate the number of digits in the number
    int numDigits = number == 0 ? 1 : static_cast<int>(log10(number)) + 1;

    // Calculate the divisor to isolate the middle n digits
    int divisor = static_cast<int>(pow(10, numDigits - startPos - n));
    // Extract the middle n digits
    int middleNDigits = (number / divisor) % static_cast<int>(pow(10, n));

    return middleNDigits;
}
