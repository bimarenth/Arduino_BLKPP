#include <ArduinoJson.h>
#include <MQTT.h>
#include <ESP8266WiFi.h>

#define BROKER_IP    "iotv2-7e62b2d2e0398d67.elb.us-east-1.amazonaws.com"
#define DEV_NAME     "esp82661.1"
#define MQTT_USER    "user1"
#define MQTT_PW      "inipassuser1"
#define SOUND_VELOCITY 0.034
#define CM_TO_INCH 0.393701

const char ssid[] = "Gudang";
const char pass[] = "gudanganenak";
String subTopic[] = {"speed", "button", "time", "movement"};
String pubTopic = "sensor";

const int trigPin = 12;
const int echoPin = 14;
const int ledMerah = 5;
long duration;
int distanceCm;
int distanceInch;

WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;
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

  if (topic == "time") {
    String status = String(doc["status"]);
    int date = doc["data"][0];
    int hour = doc["data"][1];
    int minute = doc["data"][2];

    (status == "on") ? dateSetOn(date, hour, minute) : dateSetOff(date, hour, minute);
  }

  if (topic == "speed") {
    String type = String(doc["type"]);
    int rpm = doc["data"][0];
    type.toLowerCase();

    if (type == "speed") {
      Serial.print("Wusss ");
      Serial.print(rpm);
      Serial.println(" " + type);
    } else {
      Serial.print("Wusss ");
      Serial.print(rpm);
      Serial.println(" " + type);
    }
  }


  if (topic == "button") {
    const char name = doc["name"];
    const char hw = doc["hw"];
    int state = doc["state"];

    if (String(name) == "esp8266" && String(hw) == "buildInLED") {
      (state == 1) ? digitalWrite(LED_BUILTIN, LOW) : digitalWrite(LED_BUILTIN, HIGH);
    }
    if (String(name) == "esp8266" && String(hw) == "ledMerah") {
      (state == 1) ? digitalWrite(ledMerah, HIGH) : digitalWrite(ledMerah, LOW);
    }
  }
}

void dateSetOn(int date, int hour, int minute) {
  Serial.print("ON ");
  Serial.print(date);
  Serial.print(hour);
  Serial.println(minute);
}

void dateSetOff(int date, int hour, int minute) {
  Serial.print("OFF ");
  Serial.print(date);
  Serial.print(hour);
  Serial.println(minute);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(4, INPUT_PULLUP);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ledMerah, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  if (digitalRead(4) == 1) {
    WiFi.begin(ssid, pass);
    client.begin(BROKER_IP, 1883, net);
    client.onMessage(messageReceived);
    connect();
  } else {
    Serial.println("Offline Mode");
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  //  Serial.println(digitalRead(4));

  if (client.connected()) {
    while (true) {
      client.loop();
    }
  }

  if (millis() - lastMillis > 5000) {
    Serial.println("Mode Offline");
    lastMillis = millis();
  }

  //  if (digitalRead(4) == 0){
  //    ESP.restart();
  //  }
}
