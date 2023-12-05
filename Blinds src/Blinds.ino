#include "secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <Stepper.h>
#include "Wifi.h"

const int stepsPerRevolution = 2048;  // change this to fit the number of steps per revolution
static bool prevBlindsState = false;
bool alreadyRotated = false;
#define LIGHT_SENSOR_PIN 36

// ULN2003 Motor Driver Pins
#define IN1 32
#define IN2 33
#define IN3 25
#define IN4 26

// initialize the stepper library
Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

#define AWS_IOT_PUBLISH_TOPIC "esp32_blinds"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32_sub"

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.print("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }


  Serial.println("AWS IoT Connected!");
}

void publishMessageClose() {
  StaticJsonDocument<200> doc;
  doc["Closing"] = "It's getting brighter, closing the blinds....";
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);  // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  Serial.println(jsonBuffer);
}


void publishMessageOpen() {
  StaticJsonDocument<200> doc;
  doc["Open"] = "It's getting darker, opening the blinds...";
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);  // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  Serial.println(jsonBuffer);
}



void messageHandler(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char *message = doc["message"];

  Serial.println(message);

}


void setup() {
  // initialize the serial port
  Serial.begin(115200);
  connectAWS();
  myStepper.setSpeed(10);
}

unsigned long stepperMoveStartTime = 0;
const unsigned long stepperMoveDuration = 2000;  // Adjust as needed

void loop() {
  int analogValue = analogRead(LIGHT_SENSOR_PIN);
  Serial.print("Analog Value = ");
  Serial.println(analogValue);

  bool currentState = (analogValue < 1500);

  if (currentState != prevBlindsState) {
    if (currentState) {
      Serial.println("It's getting darker, opening the blinds....");
      myStepper.setSpeed(10);
      myStepper.step(stepsPerRevolution);
      alreadyRotated = true;
      publishMessageOpen();
    } else {
      Serial.println("It's getting brighter, closing the blinds....");
      myStepper.setSpeed(10);
      myStepper.step(-stepsPerRevolution);
      alreadyRotated = false;
      publishMessageClose();
    }
    prevBlindsState = currentState;
    stepperMoveStartTime = millis();  // Record the start time for non-blocking movement
  }

  if (millis() - stepperMoveStartTime >= stepperMoveDuration) {
    // Check MQTT connection and reconnect if necessary
    if (!client.connected()) {
      Serial.println("Reconnecting to AWS IoT");
      connectAWS();
    }

    // Continue with MQTT communication or other non-blocking tasks
    client.loop();
  }

  // Add a small delay or adjust as needed
  delay(100);
}
 