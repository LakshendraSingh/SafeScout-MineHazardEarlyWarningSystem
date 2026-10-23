#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include "index_html.h"

// ==========================================
// PIN DEFINITIONS
// ==========================================
#define MOTOR_IN1 18
#define MOTOR_IN2 19
#define MOTOR_IN3 21
#define MOTOR_IN4 22

#define TRIG_PIN 5
#define ECHO_PIN 23

#define DHT_PIN 4
#define DHT_TYPE DHT11

#define MQ8_GAS_PIN 34
#define MH8_IR_PIN 35

// ==========================================
// MOTOR DIRECTION TUNING
// ==========================================
// Motor A = IN1/IN2 (left), Motor B = IN3/IN4 (right).
// Your motors are mounted mirrored, so B is inverted by default.
// If FWD now drives BACKWARD: set A to true and B to false.
// If LEFT and RIGHT are swapped: set SWAP_LEFT_RIGHT to true.
#define INVERT_MOTOR_A false
#define INVERT_MOTOR_B true
#define SWAP_LEFT_RIGHT false

// ==========================================
// HARDWARE INSTANCES & GLOBALS
// ==========================================
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

const char* AP_SSID = "ESP32_Rover_Network";
const char* AP_PASS = "12345678";

bool isAutoMode = false;

float distanceCm = 0.0;
float temperature = 0.0;
float humidity = 0.0;
int rawGasValue = 0;
int gasPercentage = 0;
bool irObstacleDetected = false;

String camIP = "";  // filled in when the ESP32-CAM registers itself

unsigned long lastSensorRead = 0;
unsigned long lastAvoidRead = 0;

// Non-blocking obstacle avoidance state machine
enum AvoidState { AV_DRIVE, AV_PAUSE1, AV_BACK, AV_PAUSE2, AV_TURN };
AvoidState avoidState = AV_DRIVE;
unsigned long avoidStateStart = 0;

// ==========================================
// MOTOR CONTROL FUNCTIONS
// dir: 1 = forward, -1 = backward, 0 = stop
// ==========================================
void motorA(int dir) {
  if (INVERT_MOTOR_A) dir = -dir;
  digitalWrite(MOTOR_IN1, dir > 0 ? HIGH : LOW);
  digitalWrite(MOTOR_IN2, dir < 0 ? HIGH : LOW);
}

void motorB(int dir) {
  if (INVERT_MOTOR_B) dir = -dir;
  digitalWrite(MOTOR_IN3, dir > 0 ? HIGH : LOW);
  digitalWrite(MOTOR_IN4, dir < 0 ? HIGH : LOW);
}

void stopMotors()   { motorA(0);  motorB(0); }
void moveForward()  { motorA(1);  motorB(1); }
void moveBackward() { motorA(-1); motorB(-1); }

void turnLeft() {
  if (SWAP_LEFT_RIGHT) { motorA(1); motorB(-1); }
  else                 { motorA(-1); motorB(1); }
}

void turnRight() {
  if (SWAP_LEFT_RIGHT) { motorA(-1); motorB(1); }
  else                 { motorA(1); motorB(-1); }
}

// ==========================================
// SENSOR READING FUNCTIONS
// ==========================================
float readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 26000);
  if (duration == 0) return 400.0;
  return (duration * 0.0343) / 2.0;
}

void readAllSensors() {
  distanceCm = readUltrasonicDistance();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;

  rawGasValue = analogRead(MQ8_GAS_PIN);
  gasPercentage = map(rawGasValue, 0, 4095, 0, 100);

  irObstacleDetected = (digitalRead(MH8_IR_PIN) == LOW);
}

// ==========================================
// NON-BLOCKING OBSTACLE AVOIDANCE
// ==========================================
void setAvoidState(AvoidState s) {
  avoidState = s;
  avoidStateStart = millis();
}

void resetAvoidance() {
  avoidState = AV_DRIVE;
  avoidStateStart = millis();
}

void handleObstacleAvoidance() {
  unsigned long now = millis();

  switch (avoidState) {
    case AV_DRIVE:
      if (now - lastAvoidRead >= 60) {
        lastAvoidRead = now;
        distanceCm = readUltrasonicDistance();
        irObstacleDetected = (digitalRead(MH8_IR_PIN) == LOW);

        if ((distanceCm < 25.0 && distanceCm > 0) || irObstacleDetected) {
          stopMotors();
          setAvoidState(AV_PAUSE1);
        } else {
          moveForward();
        }
      }
      break;

    case AV_PAUSE1:
      if (now - avoidStateStart >= 150) {
        moveBackward();
        setAvoidState(AV_BACK);
      }
      break;

    case AV_BACK:
      if (now - avoidStateStart >= 300) {
        stopMotors();
        setAvoidState(AV_PAUSE2);
      }
      break;

    case AV_PAUSE2:
      if (now - avoidStateStart >= 150) {
        turnRight();
        setAvoidState(AV_TURN);
      }
      break;

    case AV_TURN:
      if (now - avoidStateStart >= 400) {
        stopMotors();
        setAvoidState(AV_DRIVE);
      }
      break;
  }
}

// ==========================================
// ROUTE HANDLERS
// ==========================================
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleApiData() {
  String json = "{";
  json += "\"dist\":" + String(distanceCm, 1) + ",";
  json += "\"ir\":" + String(irObstacleDetected ? "true" : "false") + ",";
  json += "\"gas\":" + String(gasPercentage) + ",";
  json += "\"temp\":" + String(temperature, 1) + ",";
  json += "\"hum\":" + String(humidity, 1) + ",";
  json += "\"auto\":" + String(isAutoMode ? "true" : "false") + ",";
  json += "\"cam\":\"" + camIP + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleControl() {
  if (!isAutoMode && server.hasArg("dir")) {
    String dir = server.arg("dir");
    if (dir == "forward") moveForward();
    else if (dir == "backward") moveBackward();
    else if (dir == "left") turnLeft();
    else if (dir == "right") turnRight();
    else stopMotors();
  }
  server.send(200, "text/plain", "OK");
}

void handleToggleMode() {
  isAutoMode = !isAutoMode;
  stopMotors();
  resetAvoidance();
  server.send(200, "text/plain", "OK");
}

// The ESP32-CAM calls this after joining the network
void handleCamRegister() {
  camIP = server.client().remoteIP().toString();
  Serial.print("Camera registered at: ");
  Serial.println(camIP);
  server.send(200, "text/plain", "OK");
}

// ==========================================
// SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);
  stopMotors();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(MQ8_GAS_PIN, INPUT);
  pinMode(MH8_IR_PIN, INPUT);

  dht.begin();

  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("\n--- Private Hotspot Started ---");
  Serial.print("SSID: "); Serial.println(AP_SSID);
  Serial.print("IP Address: "); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/control", handleControl);
  server.on("/toggle-mode", handleToggleMode);
  server.on("/cam-register", handleCamRegister);
  server.begin();
}

void loop() {
  server.handleClient();

  if (millis() - lastSensorRead > 400) {
    readAllSensors();
    lastSensorRead = millis();
  }

  if (isAutoMode) {
    handleObstacleAvoidance();
  }
}
