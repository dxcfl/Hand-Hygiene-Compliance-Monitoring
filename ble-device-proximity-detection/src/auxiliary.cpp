/*
 * auxiliary.cpp
 */

#include <Arduino.h>
#include <time.h>
#include "debug2serial.h"
#include "auxiliary.h"

// Clock: Retrieves the current time from the defined NTP server
// and store into ESP32 using using configTime.
void retrieve_and_store_NTP_time(const char *ntp_server, const int tz_gmt_offset,const int tz_dst_offset)
{
  const long gmt_offset_sec = tz_gmt_offset * 3600;
  const int daylight_offset_sec = tz_dst_offset * 3600;

  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
}

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