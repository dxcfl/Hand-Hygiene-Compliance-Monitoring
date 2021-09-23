/*
 * ble_scan.h
 */

#ifndef BLE_SCAN_H
#define BLE_SCAN_H

#include <BLEScan.h>
#include <ArduinoJson.h>

#define BLE_SCAN_SETUP_ACTIVE_SCAN true // active scan uses more power, but get results faster
#define BLE_SCAN_SETUP_INTERVAL 100     // interval time to scann (ms)
#define BLE_SCAN_SETUP_WINDOW 99        // window to activly scan (ms) - less or equal interval value
#define BLE_SCAN_SETUP_DURATION 1       // seconds

BLEScan *ble_scan_init(
    const int detect_ble_scan_interval,
    const int detect_ble_scan_window);

void ble_scan_configure(
    BLEScan *pBLEScan,
    const int detect_ble_scan_interval,
    const int detect_ble_scan_window);

void ble_scan(
    BLEScan *pBLEScan,
    const int detect_ble_scan_duration,
    const String detect_name_prefix,
    const int detect_rssi_threshold,
    JsonArray *jsonArray);

#endif