#include <WiFi.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"

#define LEDS_COUNT 1
#define LEDS_PIN	8
#define CHANNEL		0

// index page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2{
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  .content {
    padding: 30px;
    max-width: 600px;
    margin: 0 auto;
  }
  .card {
    background-color: #F8F7F9;;
    box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    padding-top:10px;
    padding-bottom:20px;
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
   /*.button:hover {background-color: #0f8b8d}*/
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
  </style>
<title>ESP Web Server</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
</head>
<body>
  <div class="topnav">
    <h1>Color control</h1>
  </div>
 <div class="content">
    <div class="card">
      <input type="color" id="html5colorpicker" onchange="clickColor()" value="#ff0000" style="width:85%;height: 150px;"/>
      <p class="state">Color: <span id="state"></span></p>
      <p><button id="button" class="button">Set color</button></p>
    </div>
  </div>
<script>
  var gateway = `ws://${window.location.hostname}/ws`;
  var websocket;
  window.addEventListener('load', onLoad);
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage; // <-- add this line
  }
  function clickColor(){
    websocket.send(document.getElementById("html5colorpicker").value);
  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    document.getElementById('state').innerHTML = event.data;
    document.getElementById("html5colorpicker").value = event.data;
  }
  function onLoad(event) {
    initWebSocket();
    initButton();
  }
  function initButton() {
    document.getElementById('button').addEventListener('click', toggle);
  }
  function toggle(){
    websocket.send(document.getElementById("html5colorpicker").value);
  }
</script>
</body>
</html>
)rawliteral";

// AP config
const char *ssid = "AP_SSID";
const char *password = "AP_PASSWORD";

// Query params
const char* PARAM_RED = "red";
const char* PARAM_GREEN = "green";
const char* PARAM_BLUE = "blue";

struct LedState {
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0; 
} led_state;

IPAddress AP_LOCAL_IP(192, 168, 33, 1);
IPAddress AP_GATEWAY_IP(192, 168, 33, 254);
IPAddress AP_NETWORK_MASK(255, 255, 255, 0);

// Web server
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Led
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL);

void notifyClient(AsyncWebSocketClient *client){
  char hex[8] = {0};
  sprintf(hex,"#%02X%02X%02X",led_state.red,led_state.green,led_state.blue);
  if(client == nullptr){
    ws.textAll(hex);
  } else {
    client->text(hex);
  }
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void LedHandler(AsyncWebServerRequest *request) {
  String message;
  int value;
  led_state.red = 0;
  if (request->hasParam(PARAM_RED)) {
    value = request->getParam(PARAM_RED)->value().toInt();
    if(value < 0) value = 0;
    if(value > 255) value = 255;
    led_state.red = value;
  }

  led_state.green = 0;
  if (request->hasParam(PARAM_GREEN)) {
    value = request->getParam(PARAM_GREEN)->value().toInt();
    if(value < 0) value = 0;
    if(value > 255) value = 255;
    led_state.green = value;
  }

  led_state.blue = 0;
  if (request->hasParam(PARAM_BLUE)) {
    value = request->getParam(PARAM_BLUE)->value().toInt();
    if(value < 0) value = 0;
    if(value > 255) value = 255;
    led_state.blue = value;
  }
  strip.setLedColor(0, led_state.red, led_state.green, led_state.blue);
  notifyClient(nullptr);
  request->send(200, "text/plain", "Ok");
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    int number = (int) strtol(((char *)data + 1), NULL, 16);
    led_state.red = number >> 16 & 0xFF;
    led_state.green = number >> 8 & 0xFF;
    led_state.blue = number & 0xFF;
    strip.setLedColor(0, led_state.red, led_state.green, led_state.blue);
    notifyClient(nullptr);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClient(client);
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

void setup()
{

    Serial.begin(115200);
    Serial.println("Configuring access point...");

    WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY_IP, AP_NETWORK_MASK);
    WiFi.softAPsetHostname("espdemo");

    // To initiate the Soft AP, pause the program if the initialization process encounters an error.
    if (!WiFi.softAP(ssid, password))
    {
        Serial.println("Soft AP creation failed.");
        while (1);
    }
    Serial.print("AP Name SSID: ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("AP Hostname: ");
    Serial.println(WiFi.softAPgetHostname());
    Serial.print("AP Mac Address: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.print("AP Subnet: ");
    Serial.println(WiFi.softAPSubnetCIDR());

    // LED strip start
    strip.begin();
    strip.setLedColor(0, led_state.red, led_state.green, led_state.blue);

    initWebSocket();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/html", index_html);
    });

    server.on("/led", HTTP_GET, LedHandler);
    server.onNotFound(notFound);

    server.begin();
}

void loop()
{
  ws.cleanupClients();
}
