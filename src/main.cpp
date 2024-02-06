#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

const char* ssid = "UW MPSK";
const char* password = "n3<<K.Pprc";
#define DATABASE_URL "https://techin510-lab-project-v2-default-rtdb.firebaseio.com/" // Replace with your database URL
#define API_KEY "AIzaSyC8BLcKXQhq1XA_wfhOwIDY-02Ujx06wlQ"
#define DEEP_SLEEP_DURATION 10e6 // 10 seconds in microseconds
#define MAX_WIFI_RETRIES 5 // Maximum number of WiFi connection retries

#define SLEEP_DURATION 10000  // 10 seconds in deep sleep
#define DISTANCE_THRESHOLD 30  // Distance threshold for sleep mode in centimeters

int uploadInterval = 1000; // 1 second upload interval

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
bool deepSleepConfigured = false;  // Flag to track whether deep sleep is configured

// HC-SR04 Pins
const int trigPin = 2;
const int echoPin = 3;

const float soundSpeed = 0.034;

// Function prototypes
float measureDistance();
void connectToWiFi();
void initFirebase();
void sendDataToFirebase(float distance);

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Initial delay for stability
  delay(500);

  // Measure distance and determine whether to enter sleep mode
  float currentDistance = measureDistance();
  if (currentDistance > DISTANCE_THRESHOLD) {
    Serial.println("Entering deep sleep mode...");
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000); // in microseconds
    deepSleepConfigured = true;
    esp_deep_sleep_start();
  }

  // Initialize WiFi
  connectToWiFi();

  // Initialize Firebase
  initFirebase();
}

void loop() {
  // Measure distance
  float currentDistance = measureDistance();

  // If distance is greater than threshold and deep sleep is not configured, enter sleep mode
  if (currentDistance > DISTANCE_THRESHOLD && !deepSleepConfigured) {
    Serial.println("Entering deep sleep mode...");
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION * 1000); // in microseconds
    deepSleepConfigured = true;
    esp_deep_sleep_start();
  }

  // Send data to Firebase
  sendDataToFirebase(currentDistance);

  // Delay before next iteration
  delay(uploadInterval);
}

float measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * soundSpeed / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

void connectToWiFi() {
  Serial.println(WiFi.macAddress());
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  int wifiCnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wifiCnt++;
    if (wifiCnt > MAX_WIFI_RETRIES) {
      Serial.println("WiFi connection failed");
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign up successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase sign up failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void sendDataToFirebase(float distance) {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > uploadInterval || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    if (Firebase.RTDB.pushFloat(&fbdo, "test/distance", distance)) {
      Serial.println("Data sent to Firebase");
      Serial.print("Path: ");
      Serial.println(fbdo.dataPath());
      Serial.print("Type: ");
      Serial.println(fbdo.dataType());
    } else {
      Serial.println("Failed to send data to Firebase");
      Serial.print("Reason: ");
      Serial.println(fbdo.errorReason());
    }
  }
}