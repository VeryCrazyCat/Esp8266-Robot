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

ESP8266WiFiMulti WiFiMulti;

#define LED 2
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



WiFiClient client;
HTTPClient http;

void setup() {
    ESCLeft_value = 0;
    ESCRight_value = 0;
    // USE_SERIAL.begin(921600);
    Serial.begin(115200);

    WiFiMulti.addAP("VM7893174", "svgMrsTeacgsi8yt");

    //WiFi.disconnect();
    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
    }

    String ip = WiFi.localIP().toString();

    Serial.printf("[SETUP] WiFi Connected %s\n", ip.c_str());



    // Client address
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    pinMode(LED, OUTPUT);

    ESCLeft.attach(D3);
    ESCRight.attach(D4);
    
}
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
  ESCLeft.write(ESCLeft_value);
  ESCRight.write(ESCRight_value);
  String serverPath = serverName;
  
  SSE();
 // int httpCode = http.GET();
  

  delay(1);
}

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
