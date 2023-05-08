#include <Arduino_JSON.h>
#include <MQTT.h>
#include <ESP8266WiFi.h>
#include "RTClib.h"

#define BROKER_IP    "iot.bimarenth.my.id"
#define DEV_NAME     "esp8266_"
#define MQTT_USER    "user1"
#define MQTT_PW      "inipassuser1"
#define SOUND_VELOCITY 0.034
#define CM_TO_INCH 0.393701
#define enA 2
#define in1 15
#define in2 13

#define enB 0
#define in3 14
#define in4 12

#define trig 5
#define echo 4

const char ssid[] = "Gudang";
const char pass[] = "gudanganenak";
String mac;

String subTopic[] = {"speed", "button", "time", "movement"};
String pubTopic = "sensor";

const int trigPin = 12;
const int echoPin = 14;
const int ledMerah = 5;
long duration;
int distanceCm;
int distanceInch;


byte kondisi = 0;
byte lastday, pembanding;
unsigned long tunda;

// variable untuk set waktu robot gerak
byte perulangan, set_interval, set_jam, set_menit;
// variable atur robot
int set_jarak, set_sikat, set_kecepatan;

WiFiClient net;
MQTTClient client;
RTC_DS3231 rtc;

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
  JSONVar doc = JSON.parse(payload);

  if (JSON.typeof(doc) == "undefined") {
    Serial.println("Parsing input failed!");
    return;
  }

  if (topic == "time") {
    String status = doc["status"];
    int date = doc["data"][0]["day"];
    int month = doc["data"][0]["month"];
    int year = doc["data"][0]["year"];
    int hour = doc["data"][1]["hour"];
    int minute = doc["data"][1]["minute"];

    int day = dayOfWeek(year, month, date);
    (status == "on") ? dateSetOn(day, hour, minute) : dateSetOff(day, hour, minute);
  }

  if (topic == "speed") {
    String type = doc["type"];
    int rpm = doc["data"][0];

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
    String name = doc["name"];
    String hw = doc["hw"];
    int state = doc["state"];

    if (String(name) == "esp8266" && String(hw) == "buildInLED") {
      (state == 1) ? digitalWrite(LED_BUILTIN, LOW) : digitalWrite(LED_BUILTIN, HIGH);
    }
    if (String(name) == "esp8266" && String(hw) == "ledMerah") {
      (state == 1) ? digitalWrite(ledMerah, HIGH) : digitalWrite(ledMerah, LOW);
    }
  }
}

void dateSetOn(int day, int hour, int minute) {
  Serial.println("ON ");
  Serial.println(day);
  Serial.println(hour);
  Serial.println(minute);
}

void dateSetOff(int day, int hour, int minute) {
  Serial.println("OFF ");
  Serial.println(day);
  Serial.println(hour);
  Serial.println(minute);
}

void maju() {
  // roda maju
  //  ledcWrite(0, set_kecepatan);
  analogWrite(enA, set_kecepatan);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  // sikat arah sebaliknya
  //  ledcWrite(1, set_sikat);
  analogWrite(enB, set_sikat);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

void mundur() {
  // roda mundur
  //  ledcWrite(0, set_kecepatan);
  analogWrite(enA, set_kecepatan);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  // sikat arah sebaliknya
  //  ledcWrite(1, set_sikat);
  analogWrite(enB, set_sikat);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

// fungsi untuk update jam
void update_time(byte s_tahun, byte s_bulan, byte s_tanggal, byte s_jam, byte s_menit) {
  rtc.adjust(DateTime(s_tahun, s_bulan, s_tanggal, s_jam, s_menit, 0));
}

// fungsi untuk membaca data jarak
int jarak() {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  int duration = pulseIn(echo, HIGH);

  //perhitungan untuk dijadikan jarak
  return duration / 58.2;
}

// fungsi untuk bolak balok
void ujung(byte perulangan) {
  int ulang = perulangan * 2;
  if (jarak() > set_jarak && millis() - tunda >= 10000) {  // bila jarak lebih besar dari 4 dan selang 10dtk setelah dieksekusi
    kondisi++;                                     // mengganti kodisi
    tunda = millis();                              // menyimpan millis
  }
  // jalankan robot selama ulang lebih besar dari kondisi
  if (kondisi <= ulang + 1) {
    (kondisi % 2 == 1) ? maju() : mundur();
  } else {
    kondisi = 0;
  }
}

// fungsi untuk mengatur waktu robot jalan
void time_clean() {
  DateTime now = rtc.now();
  // mengisi variable setiap beganti hari
  if (lastday != now.day()) {
    pembanding++;
  }
  lastday = now.day();  // mengisi variable agar pembanding tindak bertambah terus

  // kondisi fungsi ujung dijalankan
  if (set_interval == pembanding && set_jam == now.hour() && set_menit == now.minute()) {
    ujung(perulangan);
    pembanding = 0; // reset variable pembanding
  }
}

int dayOfWeek(int y, int m, int d) {
  int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
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

  rtc.begin();
  // sett pin ultrasonik
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  // sett output pin driver
  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  if (digitalRead(4) == 1) {
    WiFi.begin(ssid, pass);
    mac = WiFi.macAddress();
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
