#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
 
#include "DHT.h"
#define DHTPIN 12     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22
 
// LED pins
#define GREEN_LED_PIN 25  // GPIO pin for green LED (normal conditions)
#define RED_LED_PIN 26    // GPIO pin for red LED (temperature alert)
#define BLUE_LED_PIN 27   // GPIO pin for blue LED (humidity alert)

// Threshold values
#define TEMP_MAX_THRESHOLD 30.0  // °C
#define HUMIDITY_MAX_THRESHOLD 70.0  // %
 
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
 
float h;
float t;
 
DHT dht(DHTPIN, DHTTYPE);
 
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}
 
void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}

// Update LED status based on readings
void updateLEDs(float temperature, float humidity)
{
  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    // Invalid readings - turn all LEDs off
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    return;
  }
  
  bool temp_alert = temperature > TEMP_MAX_THRESHOLD;
  bool humidity_alert = humidity > HUMIDITY_MAX_THRESHOLD;
  
  // Update LEDs based on conditions
  digitalWrite(RED_LED_PIN, temp_alert ? HIGH : LOW);
  digitalWrite(BLUE_LED_PIN, humidity_alert ? HIGH : LOW);
  
  // Green LED on only when everything is normal
  digitalWrite(GREEN_LED_PIN, (!temp_alert && !humidity_alert) ? HIGH : LOW);
}
 
void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["humidity"] = h;
  doc["temperature"] = t;
  
  // Add threshold status to the message
  doc["temperatureStatus"] = (t > TEMP_MAX_THRESHOLD) ? "Alert" : "Normal";
  doc["humidityStatus"] = (h > HUMIDITY_MAX_THRESHOLD) ? "Alert" : "Normal";
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
 
void setup()
{
  Serial.begin(115200);
  
  // Initialize LED pins
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  
  // Initially turn all LEDs off
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  
  connectAWS();
  dht.begin();
  
  // Quick LED test - blink each LED once
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(500);
  digitalWrite(GREEN_LED_PIN, LOW);
  
  digitalWrite(RED_LED_PIN, HIGH);
  delay(500);
  digitalWrite(RED_LED_PIN, LOW);
  
  digitalWrite(BLUE_LED_PIN, HIGH);
  delay(500);
  digitalWrite(BLUE_LED_PIN, LOW);
}
 
void loop()
{
  h = dht.readHumidity();
  t = dht.readTemperature();
 
  if (isnan(h) || isnan(t) || h > 100.0 || t < -40.0 || t > 80.0)  // Check if any reads failed and exit early (to try again).
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
 
  Serial.print(F("Humidity: "));
  Serial.print(h, 1);
  Serial.print(F("%  Temperature: "));
  Serial.print(t, 1);
  Serial.println(F("°C "));
  
  // Update LED indicators
  updateLEDs(t, h);
 
  publishMessage();
  client.loop();
  delay(2000);
}