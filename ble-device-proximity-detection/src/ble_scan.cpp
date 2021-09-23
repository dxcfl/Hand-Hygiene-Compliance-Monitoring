/*
 * ble_scan.cpp
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include "debug2serial.h"
#include "auxiliary.h"
#include "ble_scan.h"

BLEScan *ble_scan_init(
    const int detect_ble_scan_interval,
    const int detect_ble_scan_window)
{
    DEBUG_SERIAL_PRINTLN("BLE: Setup BLE scanning ...");
    BLEDevice::init("");
    BLEScan *pBLEScan = BLEDevice::getScan(); //create new scan
    pBLEScan->setActiveScan(BLE_SCAN_SETUP_ACTIVE_SCAN);
    ble_scan_configure(pBLEScan, detect_ble_scan_interval, detect_ble_scan_window);

    return pBLEScan;
}

void ble_scan_configure(
    BLEScan *pBLEScan,
    const int detect_ble_scan_interval,
    const int detect_ble_scan_window)
{
    DEBUG_SERIAL_PRINTLN("BLE: Configure BLE scanning ...");
    pBLEScan->setInterval(detect_ble_scan_interval);
    pBLEScan->setWindow(detect_ble_scan_window);
}

void ble_scan(
    BLEScan *pBLEScan,
    const int detect_ble_scan_duration,
    const String detect_name_prefix,
    const int detect_rssi_threshold,
    JsonArray *jsonArray)
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