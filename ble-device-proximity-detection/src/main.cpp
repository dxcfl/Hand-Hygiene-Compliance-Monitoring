/* Bluetooth LE device proximity detection
 * for a hand hygiene compliance monitoring solution
 * as project submission for the
 * Reinventing healthy spaces with AWS IoT EduKit
 * challenge at hackster.io
 * 
 * loosely based on 
 * 
 * Core2 for AWS IoT EduKit
 * Arduino Basic Connectivity Example v1.0.1
 * main.cpp
 * 
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* INCLUDES
*/

#include <Arduino.h>

#include <M5Core2.h>
#include <driver/i2s.h>
#include <Wire.h>

#include <FastLED.h>
#include <time.h>

#include <WiFi.h>

#include <ArduinoBearSSL.h>

#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <BLEScan.h>

#include "arduino_secrets.h"

#define DEBUG
#include "debug2serial.h"

#include "auxiliary.h"
#include "secure_element.h"
#include "ble_scan.h"

/* GLOBALS
*/

// WiFi setup &redentials (from arduino_secrets):
const char wifi_ssid[] = WIFI_SSID;
const char wifi_password[] = WIFI_PASS;

// AWS IoT endpoint address & device certificate (from arduino_secrets)::
#define AWS_MQTT_PORT 8883
const char endpoint_address[] = AWS_IOT_ENDPOINT_ADDRESS;
const char *certificate = THING_CERTIFICATE;

// Clients for Wi-Fi, SSL, and MQTT libraries:
WiFiClient wifi_client;
BearSSLClient ssl_client(wifi_client);
MqttClient mqtt_client(ssl_client);

// MQTT: Client ID and topics to publish subscribe to.
// Note: AWS IoT Core expects a unique client Id
// and the policy restricts which client Id's can connect
// to your broker endpoint address.
//
// NOTE: client_Id is set after the ATECC608 is initialized
// to the value of the unique chip serial number.
String client_id = "";

// Keeping track of time elapsed since last MQTT message published.
unsigned long last_publish_millis = 0;

// BLE scan settings & handle
int detect_ble_scan_duration = BLE_SCAN_SETUP_DURATION;
int detect_ble_scan_interval = BLE_SCAN_SETUP_INTERVAL;
int detect_ble_scan_window = BLE_SCAN_SETUP_WINDOW;
String detect_name_prefix = "RHS";
int detect_rssi_threshold = -50;
BLEScan *pBLEScan;

// Application setup
#define PUBLISH_INTERVAL 10000

// JSON messages: Device shadow
#define SHADOW_DOCUMENT_SIZE 1024
StaticJsonDocument<SHADOW_DOCUMENT_SIZE> shadowDocument;
// DynamicJsonDocument shadowDocument(SHADOW_DOCUMENT_SIZE);
JsonArray detected;
JsonArray accept;

/* FUNCTIONS
*/

// WiFi: Connects to the specified WiFi network using the defined
// settings & credentials, retries every 5 seconds on
// failure.
void connect_wifi(const char *ssid, const char *password)
{
  DEBUG_SERIAL_PRINT("WiFi: Attempting to connect to SSID: ");
  DEBUG_SERIAL_PRINTLN(ssid);

  while (WiFi.begin(ssid, password) != WL_CONNECTED)
  {
    DEBUG_SERIAL_PRINTLN("WiFi: Failed to connect. Retrying...");
    delay(5000);
  }
  DEBUG_SERIAL_PRINT("WiFi: Successfully connected to network ");
  DEBUG_SERIAL_PRINTLN(ssid);
}

// Subscribe to the given MQTT topic.
void subscribe_MQTT_topic(String topic)
{
  // Subscribe to an MQTT topic.
  DEBUG_SERIAL_PRINT("MQTT: Subscribe to ");
  DEBUG_SERIAL_PRINTLN(topic);
  int rc = mqtt_client.subscribe(topic);
  if (1 == rc)
  {
    DEBUG_SERIAL_PRINT("MQTT: Subscribed to ");
    DEBUG_SERIAL_PRINTLN(topic);
  }
  else
  {
    DEBUG_SERIAL_PRINT("MQTT: WARNING - Subscription failed: ");
    DEBUG_SERIAL_PRINTLN(rc);
  }
}

// Publishes the MQTT message string to the MQTT broker. The thing must
// have authorization to publish to the topic, otherwise the connection
// to AWS IoT Core will disconnect.
//
// NOTE: Use the "print" interface to send messages.
void publish_MQTT_message(String topic, const char *message)
{
  DEBUG_SERIAL_PRINTLN("MQTT Publishing message to '" + topic + "':");
  DEBUG_SERIAL_PRINTLN(message);
  DEBUG_SERIAL_PRINTLN("<<<");
  mqtt_client.beginMessage(topic, strlen(message), false, 0, false);
  mqtt_client.print(message);
  mqtt_client.endMessage();
}

// Publishes the MQTT JSON message to the MQTT broker.
void publish_MQTT_message(String topic, JsonDocument *jsonDocument)
{
  char buffer[SHADOW_DOCUMENT_SIZE];
  serializeJsonPretty(*jsonDocument, buffer);
  publish_MQTT_message(topic, buffer);
}

// Connects to the MQTT message broker, AWS IoT Core using
// the defined endpoint address at the default port 8883.
// A failed connection retries every 5 seconds.
// On a successful connection, it will then subscribe to a
// default MQTT topic, that is listening to everything
// on starting with a topic filter of the device name/.
// Changing the topic filter can cause the broker to disconnect
// the client session after successfully connecting if the thing policy
// doesn't have sufficient authorization.
//
// NOTE: You must use the ATS endpoint address.
void connect_AWS_IoT(const char *endpoint_address, const String thing)
{
  const unsigned port = 8883;
  const String mqtt_topic_shadow_get = "$aws/things/" + thing + "/shadow/get/accepted/";
  const String mqtt_topic_shadow_sent_get = "$aws/things/" + thing + "/shadow/get/";
  const String mqtt_topic_shadow_update = "$aws/things/" + thing + "/shadow/update/";

  DEBUG_SERIAL_PRINT("MQTT: Attempting to AWS IoT Core message broker at mqtt:\\\\");
  DEBUG_SERIAL_PRINT(endpoint_address);
  DEBUG_SERIAL_PRINT(":");
  DEBUG_SERIAL_PRINTLN(port);

  while (!mqtt_client.connect(endpoint_address, port))
  {
    DEBUG_SERIAL_PRINTLN("MQTT: Failed to connect to AWS IoT Core. Retrying...");
    delay(5000);
  }
  DEBUG_SERIAL_PRINTLN("MQTT: Connected to AWS IoT Core!");

  // Subscribe to "$aws/things/{thing}/shadow/get/accepted/"
  subscribe_MQTT_topic(mqtt_topic_shadow_get);

  // Subscribe to "$aws/things/{thing}/shadow/update/"
  subscribe_MQTT_topic(mqtt_topic_shadow_update);

  // Send empty message to $aws/things/{ThingName}/shadow/get
  publish_MQTT_message(mqtt_topic_shadow_sent_get, "{}");
}

void shadow_update_AWS_IoT(const String thing, JsonDocument *jsonShadow) {
  const String mqtt_topic_shadow_update = "$aws/things/" + thing + "/shadow/update/";
  publish_MQTT_message(mqtt_topic_shadow_update,jsonShadow);
}


// Callback for messages received on the subscribed MQTT
// topics. Use the Stream interface to loop until all contents
// are read.
void message_received_callback(int messageSize)
{
  // we received a message, print out the topic and contents
  DEBUG_SERIAL_PRINT("Received a message with topic '");
  DEBUG_SERIAL_PRINT(mqtt_client.messageTopic());
  DEBUG_SERIAL_PRINT("', length ");
  DEBUG_SERIAL_PRINT(messageSize);
  DEBUG_SERIAL_PRINTLN(" bytes:");

  while (mqtt_client.available())
  {
    DEBUG_SERIAL_PRINT((char)mqtt_client.read());
  }
  DEBUG_SERIAL_PRINTLN("\n");
}

void intitialize_shadow()
{
  shadowDocument.clear();
  JsonObject state = shadowDocument.createNestedObject("state");
  JsonObject state_reported = state.createNestedObject("reported");
  JsonObject state_reported_detect = state_reported.createNestedObject("detect");
  state_reported_detect["interval"] = detect_ble_scan_interval;
  state_reported_detect["window"] = detect_ble_scan_window;
  state_reported_detect["duration"] = detect_ble_scan_duration;
  state_reported_detect["name_prefix"] = detect_name_prefix;
  state_reported_detect["rssi_threshold"] = detect_rssi_threshold;
  detected = state_reported.createNestedArray("detected");
}

void update_shadow()
{
  shadowDocument.clear();
  JsonObject state = shadowDocument.createNestedObject("state");
  JsonObject state_reported = state.createNestedObject("reported");
  JsonObject state_reported_detect = state_reported.createNestedObject("detect");
  state_reported_detect["interval"] = detect_ble_scan_interval;
  state_reported_detect["window"] = detect_ble_scan_window;
  state_reported_detect["duration"] = detect_ble_scan_duration;
  state_reported_detect["name_prefix"] = detect_name_prefix;
  state_reported_detect["rssi_threshold"] = detect_rssi_threshold;
  detected = state_reported.createNestedArray("detected");
}


void setup()
{
  // DEBUG_SERIAL_INIT(115200);
  DEBUG_SERIAL_PRINTLN("APP: Initializing ...");

  // Initialize the M5Stack Core2 for AWS IoT EduKit
  bool LCDEnable = true;
  bool SDEnable = true;
  bool SerialEnable = true;
  bool I2CEnable = true;
  mbus_mode_t MBUSmode = kMBusModeOutput;
  M5.begin(LCDEnable, SDEnable, SerialEnable, I2CEnable, MBUSmode);

  // Initialize the secure element, connect to Wi-Fi, sync time
  se_initialize();
  connect_wifi(wifi_ssid, wifi_password);
  retrieve_and_store_NTP_time(DEFAULT_NTP_SERVER, DEFAULT_TZ_GMT_OFFSET, DEFAULT_TZ_DST);

  // Set a callback to get the current time
  // used to validate the servers certificate
  ArduinoBearSSL.onGetTime(get_stored_time);

  // Uses the private key slot from the secure element and the
  // certificate you pasted into arduino_secrets.h,
  ssl_client.setEccSlot(ACTA_SLOT_PRIVATE_KEY, certificate);

  // The client Id for the MQTT client. Uses the ATECC608 serial number
  // as the unique client Id, as registered in AWS IoT, and set in the
  // thing policy.
  mqtt_client.setId(se_get_id());

  // The MQTT message callback, this function is called when
  // the MQTT client receives a message on the subscribed topic
  mqtt_client.onMessage(message_received_callback);

  // Intitialize the BLE scan
  pBLEScan = ble_scan_init(detect_ble_scan_interval, detect_ble_scan_window);

  // Initialize the device shadow JSON structure
  intitialize_shadow();
}

void loop()
{
  DEBUG_SERIAL_PRINTLN("WiFi: Checking connection ...");
  // Attempt to reconnect to Wi-Fi if disconnected.
  if (WiFi.status() != WL_CONNECTED)
  {
    connect_wifi(wifi_ssid, wifi_password);
  }

  DEBUG_SERIAL_PRINTLN("MQTT: Checking AWS IoT Core connection ...");
  // Attempt to reconnect to AWS IoT Core if disconnected.
  if (!mqtt_client.connected())
  {
    connect_AWS_IoT(endpoint_address,se_get_id());
  }

  DEBUG_SERIAL_PRINTLN("MQTT: Polling for incoming messages ...");
  // Poll for new MQTT messages and send keep alive pings.
  mqtt_client.poll();

  intitialize_shadow();

  DEBUG_SERIAL_PRINTLN("BLE: Scanning ...");
  // Scan for BLE devices in close proximity.
  ble_scan(pBLEScan, detect_ble_scan_duration, detect_name_prefix, detect_rssi_threshold, &detected);

  // Publish a message every PUBLISH_INTERVAL seconds or more.
  if (millis() - last_publish_millis > PUBLISH_INTERVAL || detected.size() > 0)
  {
    if (!mqtt_client.connected())
    {
      DEBUG_SERIAL_PRINTLN("MQTT: WARNING - CONNECTION LOST!");
    }
    DEBUG_SERIAL_PRINTLN("MQTT: Publishing ...");
    last_publish_millis = millis();
    shadow_update_AWS_IoT(se_get_id(), &shadowDocument);
  }
}