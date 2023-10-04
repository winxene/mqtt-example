/*
MQTT to Control Led and send temperature, humidiy and lux 
Control Led:
* Red Led
- TurnOn: mosquitto_pub -h broker.emqx.io -t "esp_test/cmd/LedRed" -m "1"
- TurnOff: mosquitto_pub -h broker.emqx.io -t "esp_test/cmd/LedRed" -m "0"

* Yellow Led (only in ESP8266)
- TurnOn: mosquitto_pub -h broker.emqx.io -t "esp_test/cmd/LedYellow" -m "1"
- TurnOff: mosquitto_pub -h broker.emqx.io -t "esp_test/cmd/LedYellow" -m "0"

* Green Led
- TurnOn: mosquitto_pub -h broker.emqx.io -t "esp_test/cmd/LedGreen" -m "1"
- TurnOff: mosquitto_pub -h broker.emqx.io -t "esp_test/cmd/LedGreen" -m "0"

Receive sensor data:
- subcribe ke topik "esp_test/data/#
*/
#include <Arduino.h>
#include <Ticker.h>
#include <PubSubClient.h>
#if defined(ESP32)  
  #include <WiFi.h>
#endif  
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Wire.h>
#include "BH1750.h"
#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
#include "device.h"
#if defined(ESP32)
  #define LED_COUNT 3
  const uint8_t arLed[LED_COUNT] = {LED_RED, LED_YELLOW, LED_GREEN};
#endif

#if defined(ESP8266)
  #define LED_COUNT 3
  const uint8_t arLed[LED_COUNT] = {LED_RED, LED_YELLOW, LED_GREEN};
#endif

const char* ssid = "hepatitisc";
const char* password = "qwertyuiop";

#define MQTT_BROKER  "broker.emqx.io"
// #define MQTT_BROKER  "52.32.182.17" 
#define MQTT_TOPIC_PUBLISH   "esp32_test/data"
#define MQTT_TOPIC_SUBSCRIBE "esp32_test/cmd/#"  
WiFiClient wifiClient;
PubSubClient  mqtt(wifiClient);

Ticker timerPublish, ledOff;
DHTesp dht;
BH1750 lightMeter;

char g_szDeviceId[30];
void WifiConnect();
boolean mqttConnect();
void onPublishMessage();

void setup() {
  Serial.begin(9600);
  delay(100);
  pinMode(LED_BUILTIN, OUTPUT);
  for (uint8_t i=0; i<LED_COUNT; i++)
    pinMode(arLed[i], OUTPUT);
  pinMode(PIN_SW, INPUT);

  dht.setup(PIN_DHT, DHTesp::DHT11);
  Wire.begin(PIN_SDA, PIN_SCL);
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);

  Serial.printf("Free Memory: %d\n", ESP.getFreeHeap());
  WifiConnect();
  mqttConnect();
  #if defined(ESP8266)
    timerPublish.attach_ms_scheduled(5000, onPublishMessage);
  #elif defined (ESP32)
    timerPublish.attach_ms(5000, onPublishMessage);
  #endif  
}

void loop() {
    mqtt.loop();
}

//Message arrived [esp32_test/cmd/led1]: 0
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String strTopic = topic;
  int8_t idx = strTopic.lastIndexOf('/')+1;
  String strDev = strTopic.substring(idx);
  Serial.printf("==> Recv [%s]: ", topic);
  Serial.write(payload, len);
  Serial.println();
  
  byte nValue = payload[0]-'0';
  if (nValue>=0 && nValue<=1)
  {
    if (strDev=="LedRed")
      digitalWrite(LED_RED, nValue);
      Serial.println("RED");
    if (strDev=="LedGreen")
      digitalWrite(LED_GREEN, nValue);
      Serial.println("GREEN");
  // #ifdef ESP8266
    if (strDev=="LedYellow")
      digitalWrite(LED_YELLOW, nValue);
  // #endif 
  }
}

void onPublishMessage()
{
  char szTopic[50];
  char szData[10];
  
  digitalWrite(LED_BUILTIN, LED_BUILTIN_ON);
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  float lux = lightMeter.readLightLevel();
  if (dht.getStatus()==DHTesp::ERROR_NONE)
  {
    Serial.printf("Temperature: %.2f C, Humidity: %.2f %%, light: %.2f\n", 
      temperature, humidity, lux);
    sprintf(szTopic, "%s/temp", MQTT_TOPIC_PUBLISH);
    sprintf(szData, "%.2f", temperature);
    mqtt.publish(szTopic, szData);

    sprintf(szTopic, "%s/humidity", MQTT_TOPIC_PUBLISH);
    sprintf(szData, "%.2f", humidity);
    mqtt.publish(szTopic, szData);
  }
  else
    Serial.printf("Light: %.2f lx\n", lux);

  sprintf(szTopic, "%s/light", MQTT_TOPIC_PUBLISH);
  sprintf(szData, "%.2f", lux);
  mqtt.publish(szTopic, szData);

  ledOff.once_ms(100, [](){
      digitalWrite(LED_BUILTIN, LED_BUILTIN_OFF);
  });
}

boolean mqttConnect() {
#if defined(ESP32)  
  sprintf(g_szDeviceId, "esp32_%08X",(uint32_t)ESP.getEfuseMac());
#endif  
#if defined(ESP8266)  
  sprintf(g_szDeviceId, "esp8266_%08X",(uint32_t)ESP.getChipId());
#endif  

  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(mqttCallback);
  Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId);

 // Connect to MQTT Broker
  // Or, if you want to authenticate MQTT:
  boolean status = mqtt.connect(g_szDeviceId);

  if (status == false) {
    Serial.print(" fail, rc=");
    Serial.print(mqtt.state());
    return false;
  }
  Serial.println(" success");

  mqtt.subscribe(MQTT_TOPIC_SUBSCRIBE);
  Serial.printf("Subcribe topic: %s\n", MQTT_TOPIC_SUBSCRIBE);
  onPublishMessage();
  return mqtt.connected();
}

void WifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }  
  Serial.print("System connected with IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());
}

