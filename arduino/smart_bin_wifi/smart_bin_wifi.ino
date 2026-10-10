// Smart Bin, Wi-Fi version.
// The team's tested code from arduino/smart_bin/smart_bin.ino is copied unchanged
// (globals, one_section_turn, spill_trash, Homing, setup, and the Serial part of loop).
// Everything marked "Added for Wi-Fi" connects to an MQTT broker so the phone app
// can send commands from anywhere.
//
// MQTT topics, where <name> is SECRET_TOPIC_PREFIX from arduino_secrets.h:
//   <name>/cmd     phone -> bin   ENERGY, MIX, BIO, METAL, turn, spill
//   <name>/status  bin -> phone   BUSY, DONE
//   <name>/online  bin -> phone   ONLINE / OFFLINE (retained)

#include <Servo.h>
#include <Arduino.h>
#include <WiFiS3.h>               // Added for Wi-Fi
#include <ArduinoMqttClient.h>    // Added for Wi-Fi
#include "arduino_secrets.h"      // Added for Wi-Fi: Wi-Fi and broker login

Servo BaseServo;
Servo JointServo1;
Servo JointServo2;

const int BaseSERVO_PIN = 6;
const int JointServo1_Pin = 7;
const int JointServo2_Pin = 5;
const int STOP_US    = 1825;    // neutral/stop pulse width


int trigPin = 10;
int echoPin = 9;
long time;
float distance;

int home_status = 0;

int tilt_angle = 200;
int tilt_delay = 3000;

// ---------------- Added for Wi-Fi ----------------
// Turns from home to each bin, in the order ENERGY, MIX, BIO, METAL.
// Change these to match the carousel.
const int SECTIONS = 4;
const int TURNS[4] = {0, 1, 2, 3};
const int TURN_PAUSE_MS = 300;   // pause between turns

const String CMD_TOPIC = String(SECRET_TOPIC_PREFIX) + "/cmd";
const String STATUS_TOPIC = String(SECRET_TOPIC_PREFIX) + "/status";
const String ONLINE_TOPIC = String(SECRET_TOPIC_PREFIX) + "/online";
const int MQTT_PORT = 8883;      // TLS

WiFiSSLClient net;
MqttClient mqtt(net);
String pendingCommand = "";
unsigned long lastConnectTry = 0;
bool wifiWasConnected = false;
bool brokerWasConnected = false;
// -------------------------------------------------

void one_section_turn(){
  BaseServo.writeMicroseconds(1850);
  delay(500);
  BaseServo.writeMicroseconds(STOP_US);
}

void spill_trash(){
  JointServo1.writeMicroseconds(STOP_US + tilt_angle);
  JointServo2.writeMicroseconds(STOP_US - tilt_angle);
  delay(500);
  JointServo1.writeMicroseconds(STOP_US);
  JointServo2.writeMicroseconds(STOP_US);
  delay(tilt_delay);

  JointServo1.writeMicroseconds(STOP_US-tilt_angle);
  JointServo2.writeMicroseconds(STOP_US+tilt_angle);
  delay(500);
  JointServo1.writeMicroseconds(STOP_US);
  JointServo2.writeMicroseconds(STOP_US);
}

void Homing(int distance){
  const int ultrasonic_threshold = distance;

  digitalWrite(trigPin,LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(10, LOW);

  // calculating distance
  time=pulseIn(echoPin , HIGH);
  distance = time * 0.0343/2;


  if(distance >= ultrasonic_threshold && home_status == 0 ){
    BaseServo.writeMicroseconds(1900);
    delay(100);
  }
  if(distance < ultrasonic_threshold ){
    BaseServo.writeMicroseconds(STOP_US);
    delay(100);
    home_status = 1;
  }
}

// ---------------- Added for Wi-Fi ----------------

void publish(const String &topic, const String &msg, bool retain) {
  if (!mqtt.connected()) return;
  mqtt.beginMessage(topic.c_str(), retain);
  mqtt.print(msg);
  mqtt.endMessage();
}

void notify(const String &msg) {
  Serial.println(msg);
  publish(STATUS_TOPIC, msg, false);
}

// Called from mqtt.poll(). The command runs later in loop(), not inside the callback.
void onMqttMessage(int size) {
  String msg = "";
  while (mqtt.available()) msg += (char)mqtt.read();
  msg.trim();
  pendingCommand = msg;
}

// Turn to the bin, spill, then keep turning the same way until back at home.
void sortTo(int turns) {
  for (int i = 0; i < turns; i++) {
    one_section_turn();
    delay(TURN_PAUSE_MS);
  }
  spill_trash();
  int back = (SECTIONS - turns) % SECTIONS;
  for (int i = 0; i < back; i++) {
    one_section_turn();
    delay(TURN_PAUSE_MS);
  }
}

void runCommand(const String &cmd) {
  int turns = -1;
  if (cmd == "ENERGY") turns = TURNS[0];
  else if (cmd == "MIX") turns = TURNS[1];
  else if (cmd == "BIO") turns = TURNS[2];
  else if (cmd == "METAL") turns = TURNS[3];
  else if (cmd != "turn" && cmd != "spill") {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
    return;
  }

  notify("BUSY");
  if (cmd == "turn") one_section_turn();
  else if (cmd == "spill") spill_trash();
  else sortTo(turns);
  notify("DONE");
}

void printWifiInfo() {
  // The IP address can take a moment to arrive after joining.
  unsigned long t0 = millis();
  while (WiFi.localIP()[0] == 0 && millis() - t0 < 3000) delay(100);
  Serial.println("Wi-Fi connected");
  Serial.print("  Network:    ");
  Serial.println(WiFi.SSID());
  Serial.print("  IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("  Signal:     ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm (closer to 0 is stronger)");
}

// Connects Wi-Fi and the broker when either is down. Tries at most every 5 s.
void connectNetwork() {
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  if (wifiWasConnected && !wifiUp) {
    Serial.println("Wi-Fi lost, reconnecting");
    wifiWasConnected = false;
  }
  if (brokerWasConnected && !mqtt.connected()) {
    Serial.println("Broker connection lost, reconnecting");
    brokerWasConnected = false;
  }
  if (wifiUp && mqtt.connected()) return;
  if (lastConnectTry != 0 && millis() - lastConnectTry < 5000) return;
  lastConnectTry = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to Wi-Fi ");
    Serial.println(SECRET_SSID);
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi not connected yet, retrying in 5 s");
      return;
    }
    printWifiInfo();
    wifiWasConnected = true;
  }

  Serial.println("Connecting to broker");
  mqtt.setId(String(SECRET_TOPIC_PREFIX) + "-bin");   // must be unique on a shared broker
  if (strlen(SECRET_MQTT_USER) > 0) mqtt.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);
  mqtt.beginWill(ONLINE_TOPIC.c_str(), true, 1);   // broker publishes OFFLINE if we drop
  mqtt.print("OFFLINE");
  mqtt.endWill();
  if (!mqtt.connect(SECRET_MQTT_HOST, MQTT_PORT)) {
    Serial.print("Broker connect failed, error ");
    Serial.println(mqtt.connectError());
    return;
  }
  mqtt.onMessage(onMqttMessage);
  mqtt.subscribe(CMD_TOPIC.c_str());
  publish(ONLINE_TOPIC, "ONLINE", true);
  brokerWasConnected = true;
  Serial.println("Broker connected, bin online");
}
// -------------------------------------------------

void setup() {
  Serial.begin(9600);
  BaseServo.attach(BaseSERVO_PIN);
  BaseServo.writeMicroseconds(STOP_US);   // start stopped — always do this first
  delay(1000);
  JointServo1.attach(JointServo1_Pin);
  JointServo1.writeMicroseconds(STOP_US);   // start stopped — always do this first
  delay(1000);
  JointServo2.attach(JointServo2_Pin);
  JointServo2.writeMicroseconds(STOP_US);   // start stopped — always do this first
  delay(1000);

  pinMode(trigPin, OUTPUT); // SETTING OUTPUT PIN
  pinMode(echoPin, INPUT); // SETTING INPUT PIN
}

void loop() {

  Homing(5);
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove any leading/trailing whitespace

    if (command == "turn"){
      one_section_turn();
    }
    if (command == "spill"){
      spill_trash();
    }
  }

  // ---------------- Added for Wi-Fi ----------------
  // Wait until homing has finished, because connecting can block for seconds
  // and the base must not keep turning meanwhile.
  static bool homingShown = false;
  if (home_status == 0 && !homingShown) {
    Serial.println("Homing... Wi-Fi starts when the base is home");
    homingShown = true;
  }
  if (home_status == 1) {
    connectNetwork();
    mqtt.poll();
    if (pendingCommand.length() > 0) {
      String cmd = pendingCommand;
      pendingCommand = "";
      runCommand(cmd);
    }
  }
  // -------------------------------------------------

}
