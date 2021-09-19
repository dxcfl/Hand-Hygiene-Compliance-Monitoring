/* BluetoothLE device proximity detection
 * for a
 * hand hygiene compliance monitoring solution
 * as project submission for the
 * Reinventing healthy spaces with AWS IoT EduKit
 * contest at hackster.io
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

#include <Arduino.h>
#include <M5Core2.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <WiFi.h>
#include <FastLED.h>
#include <time.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoMqttClient.h>
#include "arduino_secrets.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "ArduinoJson.h"

#define DEBUG
#include "debug2serial.h"

#define DEVICE_TZ_GMT_OFFSET +2

// NTP server details.
//
// NOTE: GMT offset is in seconds, so multiply hours by
// 3600 (e.g. Pacific Time would be -8 * 3600)
const char *ntp_server = "pool.ntp.org";
const long gmt_offset_sec = DEVICE_TZ_GMT_OFFSET * 3600;
const int daylight_offset_sec = 3600;

// Credentials:
//
// WiFi setup & credentials, AWS endpoint address &
// device certificate.
const char wifi_ssid[] = WIFI_SSID;
const char wifi_password[] = WIFI_PASS;
const char endpoint_address[] = AWS_IOT_ENDPOINT_ADDRESS;
const char *certificate = THING_CERTIFICATE;

// Clients for Wi-Fi, SSL, and MQTT libraries
WiFiClient wifi_client;
BearSSLClient ssl_client(wifi_client);
MqttClient mqtt_client(ssl_client);

// The MQTT client Id used in the connection to
// AWS IoT Core. AWS IoT Core expects a unique client Id
// and the policy restricts which client Id's can connect
// to your broker endpoint address.
//
// NOTE: client_Id is set after the ATECC608 is initialized
// to the value of the unique chip serial number.
String client_id = "";
String mqtt_topic_shadow_get = "$aws/things/" + client_id + "/shadow/get/accepted/";
String mqtt_topic_shadow_sent_get = "$aws/things/" + client_id + "/shadow/get/";
String mqtt_topic_shadow_update = "$aws/things/" + client_id + "/shadow/update/";

// Used to track how much time has elapsed since last MQTT
// message publish.
unsigned long last_publish_millis = 0;

// BLE scan:
//
// BLE scan settings
#define BLE_SCAN_SETUP_ACTIVE_SCAN true // active scan uses more power, but get results faster
#define BLE_SCAN_SETUP_INTERVAL 100     // interval time to scann (ms)
#define BLE_SCAN_SETUP_WINDOW 99        // window to activly scan (ms) - less or equal interval value
#define BLE_SCAN_SETUP_DURATION 1       // seconds
//
// BLE scan handle
BLEScan *pBLEScan;

// Application setup
#define PUBLISH_INTERVAL 10000
int detect_ble_scan_duration = BLE_SCAN_SETUP_DURATION;
int detect_ble_scan_interval = BLE_SCAN_SETUP_INTERVAL;
int detect_ble_scan_window = BLE_SCAN_SETUP_WINDOW;
String detect_name_prefix = "RHSC";
int detect_rssi_threshold = -55;

// JSON message
#define SHADOW_DOCUMENT_SIZE 1024
StaticJsonDocument<SHADOW_DOCUMENT_SIZE> shadowDocument;
// DynamicJsonDocument shadowDocument(SHADOW_DOCUMENT_SIZE);
JsonArray detected;
JsonArray accept;

// Retrieves stored time_t object and returns seconds since
// Unix Epoch time.
unsigned long get_stored_time()
{
  time_t seconds_since_epoch;
  struct tm time_info;

  if (!getLocalTime(&time_info))
  {
    DEBUG_SERIAL_PRINTLN("Failed to retrieve stored time.");
    return (0);
  }

  time(&seconds_since_epoch);
  return seconds_since_epoch;
}

// Retrieves the current time from the defined NTP server.
// NOTE: Time is stored in the ESP32, not the RTC using configTime.
void get_NTP_time()
{
  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
}

// Connects to the specified Wi-Fi network using the defined
// SSID and password. A failed connection retries every 5 seconds.
void connect_wifi()
{
  DEBUG_SERIAL_PRINT("WiFi: Attempting to connect to SSID: ");
  DEBUG_SERIAL_PRINTLN(wifi_ssid);

  while (WiFi.begin(wifi_ssid, wifi_password) != WL_CONNECTED)
  {
    DEBUG_SERIAL_PRINTLN("WiFi: Failed to connect. Retrying...");
    delay(5000);
  }
  DEBUG_SERIAL_PRINT("WiFi: Successfully connected to network ");
  DEBUG_SERIAL_PRINTLN(wifi_ssid);
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
void connect_AWS_IoT()
{
#define PORT 8883
  DEBUG_SERIAL_PRINT("MQTT: Attempting to AWS IoT Core message broker at mqtt:\\\\");
  DEBUG_SERIAL_PRINT(endpoint_address);
  DEBUG_SERIAL_PRINT(":");
  DEBUG_SERIAL_PRINTLN(PORT);

  while (!mqtt_client.connect(endpoint_address, PORT))
  {
    DEBUG_SERIAL_PRINTLN("MQTT: Failed to connect to AWS IoT Core. Retrying...");
    delay(5000);
  }

  DEBUG_SERIAL_PRINTLN("MQTT: Connected to AWS IoT Core!");

  // Subscribe to an MQTT topic.
  DEBUG_SERIAL_PRINT("MQTT: Subscribe to ");
  DEBUG_SERIAL_PRINTLN(mqtt_topic_shadow_get);
  int subscribe_rc = mqtt_client.subscribe(mqtt_topic_shadow_get);
  if (1 == subscribe_rc)
  {
    DEBUG_SERIAL_PRINT("MQTT: Subscribed to ");
    DEBUG_SERIAL_PRINTLN(mqtt_topic_shadow_get);
  }
  else
  {
    DEBUG_SERIAL_PRINT("MQTT: WARNING - Subscription failed: ");
    DEBUG_SERIAL_PRINTLN(subscribe_rc);
  }

  // Send empty message to $aws/things/{ThingName}/shadow/get
  mqtt_client.beginMessage(mqtt_topic_shadow_sent_get);
  mqtt_client.print("{}");
  mqtt_client.endMessage();
}

// Publishes the MQTT message string to the MQTT broker. The thing must
// have authorization to publish to the topic, otherwise the connection
// to AWS IoT Core will disconnect.
//
// NOTE: Use the "print" interface to send messages.
void publish_MQTT_message(String topic, char *payload)
{
  DEBUG_SERIAL_PRINTLN("Publishing message to " + topic + " ...");
  DEBUG_SERIAL_PRINTLN("Message payload:");
  DEBUG_SERIAL_PRINTLN(payload);
  mqtt_client.beginMessage(topic, strlen(payload), false, 0, false);
  mqtt_client.print(payload);
  mqtt_client.endMessage();
}

// Publishes the MQTT JSON message to the MQTT broker. 
void publish_MQTT_message(String topic, JsonDocument *jsonDocument)
{
#ifdef DEBUG
  DEBUG_SERIAL_PRINTLN("MQTT: Publishing JSON message:");
  serializeJsonPretty(*jsonDocument, Serial);
  DEBUG_SERIAL_PRINTLN();
#endif
  char buffer[SHADOW_DOCUMENT_SIZE * 2];
  serializeJsonPretty(*jsonDocument, buffer);
  DEBUG_SERIAL_PRINT("Message buffer (");
  DEBUG_SERIAL_PRINT(strlen(buffer));
  DEBUG_SERIAL_PRINTLN("):");
  DEBUG_SERIAL_PRINTLN(buffer);
  
  publish_MQTT_message(topic, buffer);
  
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

// Initialize the ATECC608 secure element to use the stored private
// key in establishing TLS and securing network messages.
//
// NOTE: The definitions for I2C are in the platformio.ini file and
// not meant to be changed for the M5Stack Core2 for AWS IoT EduKit.
void atecc608_init()
{
  DEBUG_SERIAL_PRINTLN("SE: Initializing ATECC608 Secure Element ...");
  Wire.begin(ACTA_I2C_SDA_PIN, ACTA_I2C_SCL_PIN, ACTA_I2C_BAUD);

  if (!ECCX08.begin(0x35))
  {
    DEBUG_SERIAL_PRINTLN("SE: ATECC608 Secure Element initialization error!");
    while (1)
      ;
  }
  DEBUG_SERIAL_PRINT("SE: Device serial number: ");
  DEBUG_SERIAL_PRINTLN(ECCX08.serialNumber());
}

void ble_scan_init()
{
  DEBUG_SERIAL_PRINTLN("BLE: Setup BLE scanning ...");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setActiveScan(BLE_SCAN_SETUP_ACTIVE_SCAN);
  pBLEScan->setInterval(BLE_SCAN_SETUP_INTERVAL);
  pBLEScan->setWindow(BLE_SCAN_SETUP_WINDOW);
}

void ble_scan(JsonArray *jsonArray)
{
  DEBUG_SERIAL_PRINTLN("BLE: Starting BLE scan ...");
  BLEScanResults bleScanResult = pBLEScan->start(BLE_SCAN_SETUP_DURATION);
  int ble_scan_result_size = bleScanResult.getCount();
  DEBUG_SERIAL_PRINT("BLE: Scan returned ");
  DEBUG_SERIAL_PRINT(ble_scan_result_size);
  DEBUG_SERIAL_PRINTLN(" results.");

  while (ble_scan_result_size-- > 0)
  {
    BLEAdvertisedDevice bleDevice = bleScanResult.getDevice(ble_scan_result_size);
    String id = bleDevice.getAddress().toString().c_str();
    String name = bleDevice.getName().c_str();
    int rssi = bleDevice.getRSSI();

    DEBUG_SERIAL_PRINT("BLE: Scan result #");
    DEBUG_SERIAL_PRINT(ble_scan_result_size);
    DEBUG_SERIAL_PRINT(": " + name + " (" + id + ") RSSI = ");
    DEBUG_SERIAL_PRINTLN(rssi);

    if (name.startsWith(detect_name_prefix) && rssi >= detect_rssi_threshold)
    {
      DEBUG_SERIAL_PRINT("APP: Found BLE device with matching name and appropriate RSSI: ");
      DEBUG_SERIAL_PRINT(name + " (" + id + ") RSSI = ");
      DEBUG_SERIAL_PRINTLN(rssi);
      /*
      int counter = 0;
      for (JsonVariant v : *jsonArray)
      {
        JsonObject o = v.as<JsonObject>();
        String o_id = o["id"];
        if (o_id && o_id.compareTo(id) == 0)
        {
          counter = o["counter"];
          o["rssi"] = rssi;
          o["counter"] = ++counter;
          o["time"] = get_stored_time();
#ifdef DEBUG
          DEBUG_SERIAL_PRINTLN("APP: Updated data for device '" + name + "':");
          serializeJsonPretty(o, Serial);
          DEBUG_SERIAL_PRINTLN();
#endif
          break;
        }
      }
      if (counter < 1)
      {
        JsonObject jsonObject = jsonArray->createNestedObject();
        jsonObject["id"] = id;
        jsonObject["name"] = name;
        jsonObject["rssi"] = rssi;
        jsonObject["counter"] = 1;
        jsonObject["time"] = get_stored_time(); 
#ifdef DEBUG
        DEBUG_SERIAL_PRINTLN("Scan result:");
        serializeJsonPretty(jsonObject, Serial);
        DEBUG_SERIAL_PRINTLN();
#endif
        DEBUG_SERIAL_PRINTLN("BLE scan: added device.");
      }
      */
      JsonObject jsonObject = jsonArray->createNestedObject();
      jsonObject["id"] = id;
      jsonObject["rssi"] = rssi;
      jsonObject["time"] = get_stored_time();
    }
  }

#ifdef DEBUG
  DEBUG_SERIAL_PRINTLN("APP: Found BLE devices:");
  serializeJsonPretty(*jsonArray, Serial);
  DEBUG_SERIAL_PRINTLN();
#endif

  pBLEScan->clearResults();
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

void shadow_update()
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
  atecc608_init();
  connect_wifi();
  get_NTP_time();

  // Set a callback to get the current time
  // used to validate the servers certificate
  ArduinoBearSSL.onGetTime(get_stored_time);

  // Uses the private key slot from the secure element and the
  // certificate you pasted into arduino_secrets.h,
  ssl_client.setEccSlot(ACTA_SLOT_PRIVATE_KEY, certificate);

  // The client Id for the MQTT client. Uses the ATECC608 serial number
  // as the unique client Id, as registered in AWS IoT, and set in the
  // thing policy.
  client_id = ECCX08.serialNumber();
  client_id.toLowerCase();
  mqtt_client.setId(client_id);

  // The MQTT message callback, this function is called when
  // the MQTT client receives a message on the subscribed topic
  mqtt_client.onMessage(message_received_callback);

  mqtt_topic_shadow_get = "$aws/things/" + client_id + "/shadow/get/accepted";
  mqtt_topic_shadow_sent_get = "$aws/things/" + client_id + "/shadow/get";
  mqtt_topic_shadow_update = "$aws/things/" + client_id + "/shadow/update";

  ble_scan_init();

  intitialize_shadow();
}

void loop()
{
  DEBUG_SERIAL_PRINTLN("WiFi: Checking connection ...");
  // Attempt to reconnect to Wi-Fi if disconnected.
  if (WiFi.status() != WL_CONNECTED)
  {
    connect_wifi();
  }

  DEBUG_SERIAL_PRINTLN("MQTT: Checking AWS IoT Core connection ...");
  // Attempt to reconnect to AWS IoT Core if disconnected.
  if (!mqtt_client.connected())
  {
    connect_AWS_IoT();
  }

  DEBUG_SERIAL_PRINTLN("MQTT: Polling for incoming messages ...");
  // Poll for new MQTT messages and send keep alive pings.
  mqtt_client.poll();

  intitialize_shadow();

  DEBUG_SERIAL_PRINTLN("BLE: Scanning ...");
  // Scan for BLE devices in close proximity.
  ble_scan(&detected);

  // Publish a message every PUBLISH_INTERVAL seconds or more.
  if (millis() - last_publish_millis > PUBLISH_INTERVAL || detected.size() > 0)
  {
    if (!mqtt_client.connected())
    {
      DEBUG_SERIAL_PRINTLN("MQTT: WARINING - CONNECTION LOST!");
    }
    DEBUG_SERIAL_PRINTLN("MQTT: Publishing ...");
    last_publish_millis = millis();
    publish_MQTT_message(mqtt_topic_shadow_update, &shadowDocument);
  }
}