#define DEBUG_WEBSOCKETSCLIENT
#include <Arduino.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>


#define WIFI_SSID "Portillovi"
#define WIFI_PASSWORD "V3n3z*3l4"

#define WS_HOST "aphrodite-production.up.railway.app"
#define WS_PORT 443
#define WS_PATH "/conn/perrito"

#define JSON_DOC_SIZE 2048
#define MSG_SIZE 256


WiFiMulti wifiMulti;
WebSocketsClient webSocket;

void sendErrorMessage(const char *message)
{
  char msg[MSG_SIZE];
  snprintf(msg, MSG_SIZE, "{\"acrion\":\"msg\",\"body\":{\"type\":\"error\",\"error\":\"%s\"}}", message);
  webSocket.sendTXT(msg);
}

void sendOkMessage()
{
  webSocket.sendTXT("{\"action\":\"msg\",\"type\":\"status\",\"body\":\"ok\"}");
}

uint8_t toMode(const char *val)
{
  if (strcmp(val, "output") == 0)
  {
    return OUTPUT;
  }

  if (strcmp(val, "input_pullup") == 0)
  {
    return INPUT_PULLUP;
  }

  return INPUT;
}

void handleMessage(uint8_t *payload)
{
  StaticJsonDocument<JSON_DOC_SIZE> doc;

  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    sendErrorMessage(error.c_str());
    return;
  }

  if (!doc["type"].is<const char *>())
  {
    sendErrorMessage("invalid message type format");
    return;
  }

  if (strcmp(doc["type"], "cmd") == 0)
  {
    if (!doc["body"].is<JsonObject>())
    {
      sendErrorMessage("invalid command body");
      return;
    }

    if (strcmp(doc["body"]["type"], "pinMode") == 0)
    {
      /*
      Uncomment this code for better validation of pinMode command
      if (!doc["body"]["mode"].is<const char *>()) {
        sendErrorMessage("invalid pinMode mode type");
        return;
      }
      if (strcmp(doc["body"]["mode"], "input") != 0 &&
          strcmp(doc["body"]["mode"], "input_pullup") != 0 &&
          strcmp(doc["body"]["mode"], "output") != 0) {
        sendErrorMessage("invalid pinMode mode value");
        return;
      }
      */

      pinMode(doc["body"]["pin"], toMode(doc["body"]["mode"]));
      sendOkMessage();
      return;
    }

    if (strcmp(doc["body"]["type"], "digitalWrite") == 0)
    {
      digitalWrite(doc["body"]["pin"], doc["body"]["value"]);
      sendOkMessage();
      return;
    }

    if (strcmp(doc["body"]["type"], "digitalRead") == 0)
    {
      auto value = digitalRead(doc["body"]["pin"]);

      char msg[MSG_SIZE];

      sprintf(msg, "{\"action\":\"msg\",\"type\":\"output\",\"body\":%d}",
              value);

      webSocket.sendTXT(msg);
      return;
    }

    sendErrorMessage("unsupported command type");
    return;
  }

  sendErrorMessage("unsupported message type");
  return;
}

void onWSEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.println("Disconnected!");
    break;
  case WStype_CONNECTED:
    Serial.println("Connected!");
    break;
  case WStype_TEXT:
    Serial.printf("Received text: %s\n", payload);
    handleMessage(payload);
    break;
  case WStype_BIN:
    Serial.printf("Received bin: %s", payload);
    break;
  case WStype_ERROR:
    Serial.println("Error!");
    break;
  case WStype_FRAGMENT_TEXT_START:
    Serial.println("Fragment start!");
    break;
  case WStype_FRAGMENT_BIN_START:
    Serial.println("Fragment start!");
    break;
  case WStype_FRAGMENT:
    Serial.println("Fragment!");
    break;
  case WStype_FRAGMENT_FIN:
    Serial.println("Fragment end!");
    break;
  default:
    break;
  }
}

void ping() {
   if((WiFi.status() == WL_CONNECTED))
  {
    HTTPClient client;

    client.begin("https://aphrodite-production.up.railway.app/ping");
    int response = client.GET();

    if (response > 0) {
      String payload = client.getString();
      Serial.println(payload);
    }
    else {
      Serial.println("Error on request");
    }
  }
  else {
    Serial.println("Connection Lost...");
  }
  delay(1000);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  while (wifiMulti.run() != WL_CONNECTED)
  {
    Serial.printf(".");
    delay(100);
  }
  Serial.printf("\nConnected to %s\n", WIFI_SSID);
  Serial.println(WiFi.localIP());

  // Initialize WebSocket client
  webSocket.beginSSL(WS_HOST, WS_PORT, WS_PATH, "", "wss");
  webSocket.onEvent(onWSEvent);
}

void loop()
{
  digitalWrite(LED_BUILTIN, WiFi.status() == WL_CONNECTED);
  webSocket.loop();
}