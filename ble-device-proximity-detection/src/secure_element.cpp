/*
 * secure_element.cpp
 */

#include <Arduino.h>
#include <ArduinoECCX08.h>
#include "debug2serial.h"
#include "secure_element.h"


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

void se_initialize() {
  atecc608_init();
}

String se_get_id() {
 String serialNumber = ECCX08.serialNumber();
 serialNumber.toLowerCase();
 return serialNumber;
}
