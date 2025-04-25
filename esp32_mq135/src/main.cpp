#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

// MQ-135 Configuration
#define MQ135_PIN 34     // Analog pin connected to the MQ-135 sensor
#define WARMUP_TIME 20000 // 20 seconds warmup time for MQ-135 to stabilize

// AWS Configuration
#define AWS_IOT_PUBLISH_TOPIC   "esp32/airquality"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

// Global variables
float air_quality_ppm;
unsigned long sensor_warmup_time;
bool sensor_ready = false;

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

// Read the MQ-135 sensor and convert to PPM
float readMQ135Sensor() {
  // Read the analog value
  int sensorValue = analogRead(MQ135_PIN);
  
  // Convert to voltage
  float voltage = sensorValue * (3.3 / 4095.0); // ESP32 ADC is 12-bit (0-4095)
  
  // Convert voltage to PPM
  // The formula below is an approximation for CO2
  float ppm = 10.0 * (voltage * 100.0);
  
  return ppm;
}

void publishMessage()
{
  StaticJsonDocument<200> doc;
  doc["airQualityPPM"] = air_quality_ppm;

  // Add a qualitative assessment based on PPM ranges
  if (air_quality_ppm < 700) {
    doc["airQualityStatus"] = "Good";
  } else if (air_quality_ppm < 1000) {
    doc["airQualityStatus"] = "Moderate";
  } else if (air_quality_ppm < 1500) {
    doc["airQualityStatus"] = "Poor";
  } else {
    doc["airQualityStatus"] = "Unhealthy";
  }
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
 
void setup()
{
  Serial.begin(115200);
  connectAWS();
  
  // Record start time for sensor warmup
  sensor_warmup_time = millis();
  Serial.println("MQ-135 warming up...");
}
 
void loop()
{
  // Check if sensor warmup period is complete
  if (!sensor_ready && (millis() - sensor_warmup_time > WARMUP_TIME)) {
    sensor_ready = true;
    Serial.println("MQ-135 sensor ready!");
  }
  
  if (sensor_ready) {
    // Read air quality data
    air_quality_ppm = readMQ135Sensor();
    
    // Print readings to serial monitor
    Serial.print("Air Quality: ");
    Serial.print(air_quality_ppm, 1);
    Serial.println(" PPM");
    
    // Publish to AWS IoT
    publishMessage();
  } else {
    // Display warmup countdown
    Serial.print("Warming up: ");
    Serial.print((WARMUP_TIME - (millis() - sensor_warmup_time)) / 1000);
    Serial.println(" seconds remaining");
  }
  
  client.loop();
  delay(2000);
}