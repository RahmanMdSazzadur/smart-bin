#include <Servo.h>
#include <Arduino.h>

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

}