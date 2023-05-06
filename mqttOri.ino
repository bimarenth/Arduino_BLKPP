#include <ArduinoJson.h>
#include <MQTT.h>
#include <ESP8266WiFi.h>
//#include <Arduino_JSON.h>

#define BROKER_IP    "iotv2-7e62b2d2e0398d67.elb.us-east-1.amazonaws.com"
#define DEV_NAME     "esp82661.1"
#define MQTT_USER    "user1"
#define MQTT_PW      "inipassuser1"
#define SOUND_VELOCITY 0.034
#define CM_TO_INCH 0.393701

const char ssid[] = "Gudang";
const char pass[] = "gudanganenak";
String subTopic[] = {"/hello", "button"};
String pubTopic = "sensor";

const int trigPin = 12;
const int echoPin = 14;
const int ledMerah = 5;
long duration;
int distanceCm;
int distanceInch;

WiFiClient net;
MQTTClient client;

//unsigned long lastMillis = 0;
unsigned long sampling = 0;

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nconnecting...");
  while (!client.connect(DEV_NAME, MQTT_USER, MQTT_PW)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");

  for (int i = 0; i < sizeof subTopic / sizeof subTopic[0]; i++) {
    String topic = subTopic[i];
    client.subscribe(topic);
  }
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
  StaticJsonDocument<100> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  const char* name = doc["name"];
  const char* hw = doc["hw"];
  int state = doc["state"];

  if (topic == "button") {
    if (String(name) == "esp8266" && String(hw) == "buildInLED") {
      (state == 1) ? digitalWrite(LED_BUILTIN, LOW) : digitalWrite(LED_BUILTIN, HIGH);
    }
    if (String(name) == "esp8266" && String(hw) == "ledMerah") {
      (state == 1) ? digitalWrite(ledMerah, HIGH) : digitalWrite(ledMerah, LOW);
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ledMerah, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  client.begin(BROKER_IP, 1883, net);
  client.onMessage(messageReceived);
  connect();
}

void loop() {
  if (!client.connected()) {
    connect();
  }

  if (millis() - sampling > 5000) {
    StaticJsonDocument<256> doc;
    doc["device"] = "esp8266";
    doc["sensor"] = "ultrasonic";

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);
    distanceCm = duration * SOUND_VELOCITY / 2;
    distanceInch = distanceCm * CM_TO_INCH;

    JsonArray data = doc.createNestedArray("data");
    data.add(distanceCm);
    data.add(distanceInch);
    
    char out[128];
    int b = serializeJson(doc, out);
    if (!client.publish(pubTopic, out)) {
      Serial.println("Error publishing");
    }
    //    Serial.print("Uploaded total ");
    //    Serial.print(b, DEC);
    //    Serial.println(" bytes");

    sampling = millis();
  }

  client.loop();
}
