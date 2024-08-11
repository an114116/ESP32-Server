#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <Adafruit_NeoPixel.h>
#include <DHT20.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>

// Define your tasks here
void TaskBlink(void *pvParameters);
void TaskSensor(void *pvParameters);
void TasKWebserver(void *pvParameters);
void TasKAutoControl(void *pvParameters);

// Define your components here
#define LED 48
#define soilMoistureSensorPin 1
#define lightSensorPin 2

bool fanState = 0;
bool pumpState = 0;
// DHT20 dht20;
// LiquidCrystal_I2C lcd(0x27,16,2);

// WiFi
const char *ssid = "ACLAB";
const char *password = "ACLAB2023";

// MQTT Broker
const char* mqtt_server = "broker.emqx.io";
const char* mqtt_fan_topic = "esp32/fan/state";
const char* mqtt_pump_topic = "esp32/pump/state";
const char* topic = "esp32";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings;

WiFiClient espClient;
PubSubClient client(espClient);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP IOT DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
        font-family: Arial, Helvetica, sans-serif;
        display: inline-block;
        text-align: center;
    }
    h1 {
        font-size: 1.8rem;
        color: white;
    }
    .topnav {
        overflow: hidden;
        background-color: #0A1128;
    }
    body {
        margin: 0;
    }
    .content {
        padding: 50px;
    }
    .card-grid {
        max-width: 800px;
        margin: 0 auto;
        display: grid;
        grid-gap: 2rem;
        grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    }
    .card {
        background-color: white;
        box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
        margin-bottom: 20px;
    }
    .card-title {
        font-size: 1.2rem;
        font-weight: bold;
        color: #034078
    }
    .reading {
        font-size: 1.2rem;
        color: #1282A2;
    }
    .button {
    padding: 15px 50px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #0f8b8d;
    border: none;
    border-radius: 5px;
    -webkit-touch-callout: none;
    -webkit-user-select: none;
    -khtml-user-select: none;
    -moz-user-select: none;
    -ms-user-select: none;
    user-select: none;
    -webkit-tap-highlight-color: rgba(0,0,0,0);
   }
   .button:active {
     background-color: #0f8b8d;
     box-shadow: 2 2px #CDCDCD;
     transform: translateY(2px);
   }
   .state {
     font-size: 1.5rem;
     color:#8c8c8c;
     font-weight: bold;
   }
   .control-panel {
     display: flex;
     justify-content: space-around;
   }
  </style>
</head>
<body>
  <div class="topnav">
      <h1>SENSOR READINGS (WEBSOCKET)</h1>
  </div>
  <div class="contents">
      <div class="card-grid">
          <div class="card">
              <p class="card-title"><i class="fas fa-thermometer-threequarters" style="color:#059e8a;"></i> Temperature</p>
              <p class="reading"><span id="temperature"></span> &deg;C</p>
          </div>
          <div class="card">
              <p class="card-title"> Humidity</p>
              <p class="reading"><span id="humidity"></span> &percnt;</p>
          </div>
          <div class="card">
              <p class="card-title"> Light Intensity</p>
              <p class="reading"><span id="light"></span> lux</p>
          </div>
          <div class="card">
              <p class="card-title"> Soil Moisture</p>
              <p class="reading"><span id="soilMoisture"></span></p>
          </div>
      </div>
      <div class="content">
        <div class="control-panel">
          <div class="card">
            <h2>Output - FAN</h2>
            <p class="state">State: <span id="fanState">%FAN_STATE%</span></p>
            <p><button id="fanButton" class="button">Toggle Fan</button></p>
          </div>
          <div class="card">
            <h2>Output - PUMP</h2>
            <p class="state">State: <span id="pumpState">%PUMP_STATE%</span></p>
            <p><button id="pumpButton" class="button">Toggle Pump</button></p>
          </div>
        </div>
      </div>
  </div>
  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    window.addEventListener('load', onload);

    function onload(event) {
        initWebSocket();
        initButtons();
    }

    function initWebSocket() {
        console.log('Trying to open a WebSocket connection…');
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
    }

    function onOpen(event) {
        console.log('Connection opened');
        getReadings();
    }

    function onClose(event) {
        console.log('Connection closed');
        setTimeout(initWebSocket, 2000);
    }

    function onMessage(event) {
        console.log(event.data);
        if (event.data == "FAN_ON" || event.data == "FAN_OFF") {
            var state = (event.data == "FAN_ON") ? "ON" : "OFF";
            document.getElementById('fanState').innerHTML = state;
        } else if (event.data == "PUMP_ON" || event.data == "PUMP_OFF") {
            var state = (event.data == "PUMP_ON") ? "ON" : "OFF";
            document.getElementById('pumpState').innerHTML = state;
        } else {
            var myObj = JSON.parse(event.data);
            document.getElementById('temperature').innerHTML = myObj['Temperature'];
            document.getElementById('humidity').innerHTML = myObj['Humidity'];
            document.getElementById('light').innerHTML = myObj['Light'];
            document.getElementById('soilMoisture').innerHTML = myObj['SoilMoisture'];
        } 
    }

    function initButtons() {
      document.getElementById('fanButton').addEventListener('click', toggleFan);
      document.getElementById('pumpButton').addEventListener('click', togglePump);
    }

    function getReadings(){
      websocket.send("getReadings");
    }

    function toggleFan(){
      websocket.send('toggleFan');
    }

    function togglePump(){
      websocket.send('togglePump');
    }
  </script>
</body>
</html>
)rawliteral";

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String sensorReadings) {
  ws.textAll(sensorReadings);
}

void notifyClient() {
  String fanStateStr = fanState ? "FAN_ON" : "FAN_OFF";
  String pumpStateStr = pumpState ? "PUMP_ON" : "PUMP_OFF";
  ws.textAll(fanStateStr);
  ws.textAll(pumpStateStr);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);

  if (String(topic) == mqtt_fan_topic) {
    if (message == "ON") {
      fanState = true;
    } else if (message == "OFF") {
      fanState = false;
    }
    notifyClient();
  }

  if (String(topic) == mqtt_pump_topic) {
    if (message == "ON") {
      pumpState = true;
    } else if (message == "OFF") {
      pumpState = false;
    }
    notifyClient();
  }
}

void MQTTBroker() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.print("Connecting to public EMQX MQTT broker\n");
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public EMQX MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public EMQX MQTT broker connected");
      delay(5000);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  client.publish(topic, "Hi, I'm ESP32 ^^");
  client.subscribe(mqtt_fan_topic);
  client.subscribe(mqtt_pump_topic);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = String((char*)data);

    if (message == "toggleFan") {
      fanState = !fanState;
      String fanStateStr = fanState ? "FAN_ON" : "FAN_OFF";
      client.publish(mqtt_fan_topic, fanStateStr.c_str());
      notifyClient();
    }
    if (message == "togglePump") {
      pumpState = !pumpState;
      String pumpStateStr = pumpState ? "PUMP_ON" : "PUMP_OFF";
      client.publish(mqtt_pump_topic, pumpStateStr.c_str());
      notifyClient();
    }
    if (message == "getReadings") {
      // dht20.read();
      String temperature = String(28);
      String humidity = String(60);
      String light = String(analogRead(lightSensorPin));
      String soil = String(analogRead(soilMoistureSensorPin));

      JSONVar readings;
      readings["Temperature"] = temperature;
      readings["Humidity"] = humidity;
      readings["Light"] = light;
      readings["SoilMoisture"] = soil;

      String sensorData = JSON.stringify(readings);
      notifyClients(sensorData);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%lu connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%lu disconnected\n", client->id());
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

String processor(const String& var){
  if(var == "FAN_STATE"){
    return fanState ? "ON" : "OFF";
  } else if (var == "PUMP_STATE") {
    return pumpState ? "ON" : "OFF";
  }
  return String();
}

void setup() {
  Serial.begin(115200); 
  // dht20.begin();
  // lcd.init();
  // lcd.backlight();

  initWiFi();
  MQTTBroker();
  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  server.begin();

  xTaskCreate( TaskBlink, "Task Blink" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate( TaskSensor, "Task Sensor" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate( TasKWebserver, "Task Webserver", 2048, NULL, 2, NULL);
  xTaskCreate( TasKAutoControl, "Task Auto Control", 2048, NULL, 2, NULL);

  Serial.printf("Basic Multi Threading Arduino Example\n");
}

void loop() {
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskBlink(void *pvParameters) {  
  pinMode(LED, OUTPUT);
  Serial.println("Task Blink");
  while(1) {                          
    digitalWrite(LED, HIGH);
    delay(200);
    digitalWrite(LED, LOW);
    delay(200);
  }
}

void TaskSensor(void *pvParameters) {
  pinMode(soilMoistureSensorPin, INPUT);
  pinMode(lightSensorPin, INPUT);
  Serial.println("Task Sensor");
  while(1) {                          
    String temperature = String(28);
    String humidity = String(60);
    String light = String(analogRead(lightSensorPin));
    String soil = String(analogRead(soilMoistureSensorPin));

    JSONVar readings;
    readings["Temperature"] = temperature;
    readings["Humidity"] = humidity;
    readings["Light"] = light;
    readings["SoilMoisture"] = soil;

    String sensorData = JSON.stringify(readings);
    notifyClients(sensorData);
    delay(1000);
  }
}

void TasKWebserver(void *pvParameters) {
  Serial.println("Task Webserver");
  while(1) {
    client.loop();
    ws.cleanupClients();
    delay(1000);
  }
}

void TasKAutoControl(void *pvParameters){
  pinMode(soilMoistureSensorPin, INPUT);
  pinMode(lightSensorPin, INPUT);
  Serial.println("Task Auto Control");
  while(1) {
    // dht20.read();
    float temperature = 28;
    float humidity = 60;
    int lightIntensity = analogRead(lightSensorPin);
    int soilMoisture = analogRead(soilMoistureSensorPin);
    // Automatic control logic
    if (temperature > 28) {
      fanState = true;
    } else {
      fanState = false;
    }

    if (soilMoisture < 60) {
      pumpState = true;
    } else if (soilMoisture > 70) {
      pumpState = false;
    }

    // Notify MQTT Broker and WebSocket clients
    if (fanState) {
      client.publish(mqtt_fan_topic, "FAN_ON");
    } else {
      client.publish(mqtt_fan_topic, "FAN_OFF");
    }

    if (pumpState) {
      client.publish(mqtt_pump_topic, "PUMP_ON");
    } else {
      client.publish(mqtt_pump_topic, "PUMP_OFF");
    }

    notifyClient();
    delay(1000);
  }
}