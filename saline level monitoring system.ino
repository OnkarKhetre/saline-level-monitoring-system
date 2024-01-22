#include "FS.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino.h>
#include "HX711.h"
#include <Servo.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <UrlEncode.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"

#define REPORTING_PERIOD_MS 10000

PulseOximeter pox;

int res = 1;
int res1 = 1;

uint32_t tsLastReport = 0;

Servo servo;

const char* ssid = "OPPO A15ss";
const char* password = "Onkar@90";

String phoneNumber = "+919075371782";
String apiKey = "7561076";

void sendMessage(String message) {
  String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpResponseCode = http.POST(url);
  if (httpResponseCode == 200) {
    Serial.print("\nAlert Message sent successfully\n");
  } else {
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

const int LOADCELL_DOUT_PIN = 12;
const int LOADCELL_SCK_PIN = 13;

HX711 scale;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const char* AWS_endpoint = "ajujtbnnc644w-ats.iot.us-east-2.amazonaws.com";

void onBeatDetected() {
  Serial.println("Beat!");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

// Thresholds for Pulse Oximeter readings
float minHeartRate = 60.0;
float maxHeartRate = 100.0;
float minSpo2 = 95.0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  espClient.setX509Time(timeClient.getEpochTime());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESPthing")) {
      Serial.println("connected");
      client.publish("outTopic", "hello world");
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      char buf[256];
      espClient.getLastSSLError(buf, 256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);
      delay(5000);
    }
  }
}

void setup() {
  if (!pox.begin()) {
    Serial.println("FAILED to initialize PulseOximeter");
    for (;;);
  } else {
    Serial.println("PulseOximeter Initialized");
  }
  delay(7000);

  Serial.println("HX711 Demo");
  Serial.println("Initializing the scale");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  Serial.println("Before setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read());
  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));
  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));
  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);
  scale.set_scale(482.312);
  scale.tare();
  Serial.println("After setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read());
  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));
  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));
  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);

  Serial.println("Readings:");
  servo.attach(0);
  servo.write(90);
  delay(2000);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(LED_BUILTIN, OUTPUT);
  setup_wifi();
  delay(1000);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    Serial.println("Failed to open cert file");
  } else
    Serial.println("Success to open cert file");
  delay(1000);

  if (espClient.loadCertificate(cert))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");

  File private_key = SPIFFS.open("/private.der", "r");
  if (!private_key) {
    Serial.println("Failed to open private cert file");
  } else
    Serial.println("Success to open private cert file");
  delay(1000);

  if (espClient.loadPrivateKey(private_key))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");

  File ca = SPIFFS.open("/ca.der", "r");
  if (!ca) {
    Serial.println("Failed to open ca ");
  } else
    Serial.println("Success to open ca");
  delay(1000);

  if (espClient.loadCACert(ca))
    Serial.println("ca loaded");
  else
    Serial.println("ca failed");

  Serial.print("Heap: ");
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - tsLastReport >= REPORTING_PERIOD_MS) {
    tsLastReport = currentMillis;

    // Simulate Pulse Oximeter readings when a finger is detected
    bool fingerDetected = digitalRead(D2) == HIGH;
    float simulatedHeartRate = fingerDetected ? random(70, 86) : 0;
    float simulatedSpo2 = fingerDetected ? random(90, 99) : 0;

    // Check if simulated Pulse Oximeter readings are valid
    if (simulatedHeartRate > 0 && simulatedSpo2 > 0) {
      Serial.print("Simulated Heart rate: ");
      Serial.print(simulatedHeartRate);
      Serial.print(" bpm / Simulated SpO2: ");
      Serial.print(simulatedSpo2);
      Serial.println("%");

      // Check if simulated heart rate is outside the normal range
      if (simulatedHeartRate < minHeartRate || simulatedHeartRate > maxHeartRate) {
        sendMessage("Alert! Abnormal Heart Rate detected");
      }

      // Check if simulated SpO2 is below the normal range
      if (simulatedSpo2 < minSpo2) {
        sendMessage("Alert! Low Blood Oxygen (SpO2) detected");
      }
    } else {
      Serial.println("Invalid Simulated Heart rate or SpO2 values");
    }

    float initial = 135.0;
    float weight = abs(scale.get_units());
    float weights = abs(scale.get_units(10));
    float per = (weight / initial) * 100.0;

    Serial.print("Saline level (grams): ");
    Serial.print(weight, 1);
    Serial.print("\t| Average (grams): ");
    Serial.println(weights, 5);

    scale.power_down();
    delay(5000);
    scale.power_up();

    if (per < 15.0 && per > 10.0) {
      delay(1000);
      servo.write(180);
      if (res > 0) {
        sendMessage("Alert! Saline level is above 15 percent \n Saline flow is being stopped");
        res = 0;
      }
    } else if (per > 15.0 && per < 30.0) {
      delay(1000);
      servo.write(90);
      if (res1 > 0) {
        sendMessage("Alert! Saline level is below 30 percent");
        res1 = 0;
      }
    } else {
      delay(1000);
      servo.write(90);
    }

    snprintf(msg, 75, "{\"Saline level (reading #%ld)\": \"%f percent\"}", value, per);
    Serial.print("Publish Saline message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);

    snprintf(msg, 75, "{\"Heart Rate (reading #%ld)\": \"%f bpm\"}", value, simulatedHeartRate);
    Serial.print("Publish Heart Rate message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);

    snprintf(msg, 75, "{\"SpO2 (reading #%ld)\": \"%f percent\"}", value, simulatedSpo2);
    Serial.print("Publish SpO2 message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);

    Serial.print("Heap: ");
    Serial.println(ESP.getFreeHeap());
  }

  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}
