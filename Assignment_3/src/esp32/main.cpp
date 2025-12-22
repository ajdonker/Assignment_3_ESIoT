#include<Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "devices/Sonar.h"
#define MSG_BUFFER_SIZE  50

/* wifi network info */
//his my own wifi network at home i guess ?? how to know what to enter here 
const char* ssid = "NOVA_55C8"; 
const char* password = "feel8266"; 
//const char* password = "wrong_password"; 
/* MQTT server address */
const char* mqtt_server = "broker.mqtt-dashboard.com";

/* MQTT topic */
const char* topic = "esiot-2025/blagoja";

/* MQTT client management */

WiFiClient espClient;
PubSubClient client(espClient);
enum class TmsState{NORMAL,TIMEOUT} state = TmsState::NORMAL; // async change states callbacks. if in normal and no msg in time T2 timeout 
// if in TIMEOUT msg arrives go to NORMAL
unsigned long lastPublishTime = 0;
unsigned long lastRecvTime = 0;
char msg[MSG_BUFFER_SIZE];
int value = 0;
const unsigned long FREQ = 4000;
const unsigned long TIMEOUT_TIME = 20000;
const uint8_t GREEN_LED_PIN = 23;
const uint8_t RED_LED_PIN = 22;
const uint8_t TRIG_PIN = 21;
const uint8_t ECHO_PIN = 19;
Sonar sonar(ECHO_PIN, TRIG_PIN, 30000);
void setup_wifi() {
  delay(10);
  Serial.println(String("Connecting to ") + ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/* MQTT subscribing callback */

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println(String("Message arrived on [") + topic + "] len: " + length + " txt: " + String((char*)payload, length) );
  state = TmsState::NORMAL;
  lastRecvTime = millis();
}

void reconnect() {
  
  // Loop until we're reconnected
  
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = String("esiot-2025-client-")+String(random(0xffff), HEX);

    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
    Serial.begin(115200);
    setup_wifi();
    randomSeed(micros());
    pinMode(RED_LED_PIN,OUTPUT);
    pinMode(GREEN_LED_PIN,OUTPUT);
    digitalWrite(RED_LED_PIN,HIGH);
    digitalWrite(GREEN_LED_PIN,LOW);
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    state = TmsState::TIMEOUT;
    reconnect();
    return;
  }

  client.loop();

  unsigned long now = millis();

  // Periodic sampling + sending
  if (now - lastPublishTime > FREQ) {
    lastPublishTime = now;

    float d = sonar.getDistance();
    snprintf(msg, MSG_BUFFER_SIZE, "distance=%.3f", d);

    if (client.publish(topic, msg)) {
      state = TmsState::NORMAL;   // sending OK
    } else {
      state = TmsState::TIMEOUT;  // sending failed
    }
  }

  // LED logic purely reflects state
  switch (state) {
    case TmsState::NORMAL:
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      break;

    case TmsState::TIMEOUT:
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, HIGH);
      break;
  }
}
