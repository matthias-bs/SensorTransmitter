///////////////////////////////////////////////////////////////////////////////////////////////////
// SensorTransmitter.h
//
// Bresser 5-in-1/6-in-1/7-in-1 868 MHz Sensor Radio Transmitter
// based on CC1101 or SX1276/RFM95W and ESP32/ESP8266
//
// https://github.com/matthias-bs/SensorTransmitter
//
// created: 11/2023
//
//
// MIT License
//
// Copyright (c) 2023 Matthias Prinke
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// History:
// 20231111 Created based on 
//          https://github.com/jgromes/RadioLib/blob/master/examples/SX127x/SX127x_Transmit_Blocking/SX127x_Transmit_Blocking.ino
// 20231112 Added utilization of class WeatherSensor
//          Added JSON string as payload source
// 20231113 Added JSON string input from serial console
//          Added TRANSCEIVER_CHIP
// 20231114 Added enum Encoders
// 20241227 Added LilyGo T3 S3 SX1262/SX1276/LR1121
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(SENSOR_TRANSMITTER_H)
#define SENSOR_TRANSMITTER_H

#include <Arduino.h>

#define MAX_SENSORS_DEFAULT 1       //!< WeatherSensor - no. of sensors
#define WIND_DATA_FLOATINGPOINT     //!< WeatherSensor - wind data type

//!< Select one of the following data sources
//#define DATA_RAW                  //!< payload from raw data
//#define DATA_GEN                  //!< payload from WeatherSensor::genMessage()
//#define DATA_JSON_CONST             //!< payload from JSON constant string
#define DATA_JSON_INPUT             //!< payload from JSON serial console input

#define TX_INTERVAL 30              //!< transmit interval in seconds

enum struct Encoders {
    ENC_BRESSER_5IN1,
    ENC_BRESSER_6IN1,
    ENC_BRESSER_7IN1,
    ENC_BRESSER_LEAKAGE,
    ENC_BRESSER_LIGHTNING
};

// ------------------------------------------------------------------------------------------------
// --- Board ---
// ------------------------------------------------------------------------------------------------
// Use pinning for LoRaWAN Node


// LILIGO TTGO LoRaP32 board with integrated RF tranceiver (SX1276)
// See pin definitions in
// https://github.com/espressif/arduino-esp32/tree/master/variants/ttgo-lora32-*
// and
// https://www.thethingsnetwork.org/forum/t/big-esp32-sx127x-topic-part-2/11973

// This define is set by selecting "Board: TTGO LoRa32-OLED" / "Board Revision: TTGO LoRa32 V1 (No TFCard)"
// in the Arduino IDE:
//#define ARDUINO_TTGO_LoRa32_V1

// This define is set by selecting "Board: TTGO LoRa32-OLED" / "Board Revision: TTGO LoRa32 V2"
// in the Arduino IDE:
//#define ARDUINO_TTGO_LoRa32_V2

// This define is set by selecting "Board: TTGO LoRa32-OLED" / "Board Revision: TTGO LoRa32 V2.1 (1.6.1)"
// in the Arduino IDE:
//#define ARDUINO_TTGO_LoRa32_V21new

// This define is set by selecting "Board: Heltec Wireless Stick"
// in the Arduino IDE:
//#define ARDUINO_heltec_wireless_stick

// This define is set by selecting "Board: Heltec WiFi LoRa 32(V2)"
// in the Adruino IDE:
//#define ARDUINO_heltec_wifi_lora_32_V2

// Adafruit Feather ESP32S2 with RFM95W "FeatherWing" ADA3232
// https://github.com/espressif/arduino-esp32/blob/master/variants/adafruit_feather_esp32s2/pins_arduino.h
//
// This define is set by selecting "Adafruit Feather ESP32-S2" in the Arduino IDE:
//#define ARDUINO_ADAFRUIT_FEATHER_ESP32S2

// Adafruit Feather ESP32 with RFM95W "FeatherWing" ADA3232
// https://github.com/espressif/arduino-esp32/blob/master/variants/feather_esp32/pins_arduino.h
//
// This define is set by selecting "Adafruit ESP32 Feather" in the Arduino IDE:
//#define ARDUINO_FEATHER_ESP32

// Adafruit Feather RP2040 with RFM95W "FeatherWing" ADA3232
// https://github.com/espressif/arduino-esp32/blob/master/variants/feather_esp32/pins_arduino.h
//
// This define is set by selecting "Adafruit Feather RP2040" in the Arduino IDE:
//#define ARDUINO_ADAFRUIT_FEATHER_RP2040

// DFRobot Firebeetle32
// https://github.com/espressif/arduino-esp32/tree/master/variants/firebeetle32/pins_arduino.h
//
// This define (not very specific...) is set by selecting "FireBeetle-ESP32" in the Arduino IDE:
//#define ARDUINO_ESP32_DEV

#if defined(ARDUINO_TTGO_LoRa32_V1)
    #pragma message("ARDUINO_TTGO_LoRa32_V1 defined; using on-board transceiver")
    #define USE_SX1276

#elif defined(ARDUINO_TTGO_LoRa32_V2)
    #pragma message("ARDUINO_TTGO_LoRa32_V2 defined; using on-board transceiver")
    #pragma message("LoRa DIO1 must be wired to GPIO33 manually!")
    #define USE_SX1276

#elif defined(ARDUINO_TTGO_LoRa32_v21new)
    #pragma message("ARDUINO_TTGO_LoRa32_V21new defined; using on-board transceiver")
    #define USE_SX1276

#elif defined(ARDUINO_LILYGO_T3S3_SX1262)
    // https://github.com/espressif/arduino-esp32/blob/master/variants/lilygo_t3_s3_sx1262/pins_arduino.h
    #pragma message("ARDUINO_LILYGO_T3S3_SX1262 defined; using on-board transceiver")
    #define USE_SX1262
    #define PIN_RECEIVER_CS   LORA_CS
    #define PIN_RECEIVER_IRQ  LORA_IRQ
    #define PIN_RECEIVER_GPIO LORA_BUSY
    #define PIN_RECEIVER_RST  LORA_RST

#elif defined(ARDUINO_LILYGO_T3S3_SX1276)
    // https://github.com/espressif/arduino-esp32/blob/master/variants/lilygo_t3_s3_sx127x/pins_arduino.h
    #pragma message("ARDUINO_LILYGO_T3S3_SX1276 defined; using on-board transceiver")
    #define USE_SX1276
    #define PIN_RECEIVER_CS   LORA_CS
    #define PIN_RECEIVER_IRQ  LORA_IRQ
    #define PIN_RECEIVER_GPIO LORA_BUSY
    #define PIN_RECEIVER_RST  LORA_RST

#elif defined(ARDUINO_LILYGO_T3S3_LR1121)
    // https://github.com/espressif/arduino-esp32/blob/master/variants/lilygo_t3_s3_lr1121/pins_arduino.h
    #pragma message("ARDUINO_LILYGO_T3S3_LR1121 defined; using on-board transceiver")
    #define USE_LR1121
    #define PIN_RECEIVER_CS   LORA_CS
    #define PIN_RECEIVER_IRQ  LORA_IRQ
    #define PIN_RECEIVER_GPIO LORA_BUSY
    #define PIN_RECEIVER_RST  LORA_RST

#elif defined(ARDUINO_HELTEC_WIRELESS_STICK)
    #pragma message("ARDUINO_heltec_wireless_stick defined; using on-board transceiver")
    #define USE_SX1276

#elif defined(ARDUINO_HELTEC_WIFI_LORA_32_V2)
    #pragma message("ARDUINO_heltec_wifi_lora_32_V2 defined; using on-board transceiver")
    #define USE_SX1276

#elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
    #pragma message("ARDUINO_ADAFRUIT_FEATHER_ESP32S2 defined; assuming RFM95W FeatherWing will be used")
    #define USE_SX1276

#elif defined(ARDUINO_FEATHER_ESP32)
    #pragma message("ARDUINO_FEATHER_ESP32 defined; assuming RFM95W FeatherWing will be used")
    #define USE_SX1276
    #pragma message("Required wiring: A to RST, B to DIO1, D to DIO0, E to CS")

#elif defined(ARDUINO_AVR_FEATHER32U4)
    #pragma message("ARDUINO_AVR_FEATHER32U4 defined; assuming this is the Adafruit Feather 32u4 RFM95 LoRa Radio")
    #define USE_SX1276

#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
    #pragma message("ARDUINO_ADAFRUIT_FEATHER_RP2040 defined; assuming assuming RFM95W FeatherWing will be used")
    #define USE_SX1276
    #pragma message("Required wiring: A to RST, B to DIO1, D to DIO0, E to CS")

#elif defined(ARDUINO_DFROBOT_FIREBEETLE_ESP32)
    //#define LORAWAN_NODE
    #define FIREBEETLE_ESP32_COVER_LORA

    #if defined(FIREBEETLE_ESP32_COVER_LORA)
        #pragma message("FIREBEETLE_ESP32_COVER_LORA defined; assuming this is a FireBeetle ESP32 with FireBeetle Cover LoRa")
        #define USE_SX1276
        #pragma message("Required wiring: D2 to RESET, D3 to DIO0, D4 to CS, D5 to DIO1")

    #elif defined(LORAWAN_NODE) 
        #pragma message("LORAWAN_NODE defined; assuming this is the LoRaWAN_Node board (DFRobot Firebeetle32 + Adafruit RFM95W LoRa Radio)")
        #define USE_SX1276

    #else
        #pragma message("ARDUINO_DFROBOT_FIREBEETLE_ESP32 defined; select either LORAWAN_NODE or FIREBEETLE_ESP32_COVER_LORA manually!")
        
    #endif

#endif


// ------------------------------------------------------------------------------------------------
// --- Radio Transceiver ---
// ------------------------------------------------------------------------------------------------
#if defined(USE_CC1101)
    #define TRANSCEIVER_CHIP "[CC1101]"
#elif defined(USE_SX1276)
    #define TRANSCEIVER_CHIP "[SX1276]"
#elif defined(USE_SX1262)
    #define TRANSCEIVER_CHIP "[SX1262]"
#elif defined(USE_LR1121)
    #define TRANSCEIVER_CHIP "[LR1121]"
#else
    #error "Either USE_CC1101, USE_SX1276, USE_SX1262 or USE_LR1121 must be defined!"
#endif

// Arduino default SPI pins
//
// Board   SCK   MOSI  MISO
// ESP8266 D5    D7    D6
// ESP32   D18   D23   D19
#if defined(LORAWAN_NODE)
    // Use pinning for LoRaWAN_Node (https://github.com/matthias-bs/LoRaWAN_Node)
    #define PIN_TRANSCEIVER_CS   14

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  4

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO 16

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  12

#elif defined(FIREBEETLE_ESP32_COVER_LORA)
    #define PIN_TRANSCEIVER_CS   27 // D4

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  26 // D3

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO 9  // D5

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  25 // D2

#elif defined(ARDUINO_TTGO_LoRa32_V1) || defined(ARDUINO_TTGO_LoRa32_V2)
    // Use pinning for LILIGO TTGO LoRa32-OLED
    #define PIN_TRANSCEIVER_CS   LORA_CS

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  LORA_IRQ

    // CC1101: GDO2 / RFM95W/SX127x: G1
    // n.c. on v1/v2?, LORA_D1 on v21
    #define PIN_TRANSCEIVER_GPIO 33

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  LORA_RST

#elif defined(ARDUINO_TTGO_LoRa32_v21new)
    // Use pinning for LILIGO TTGO LoRa32-OLED V2.1 (1.6.1)
    // Same pinout for Heltec Wireless Stick
    #define PIN_TRANSCEIVER_CS   LORA_CS

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  LORA_IRQ

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO LORA_D1

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  LORA_RST

#elif defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wifi_lora_32_V2)
    // Use pinning for Heltec Wireless Stick or WiFi LoRa32 V2, respectively
    #define PIN_TRANSCEIVER_CS   SS

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  DIO0

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO DIO1

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  RST_LoRa

#elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
    // Use pinning for Adafruit Feather ESP32S2 with RFM95W "FeatherWing" ADA3232
    #define PIN_TRANSCEIVER_CS   6

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  5

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO 11

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  9

#elif defined(ARDUINO_FEATHER_ESP32)
    // Use pinning for Adafruit Feather ESP32 with RFM95W "FeatherWing" ADA3232
    #define PIN_TRANSCEIVER_CS   14

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  32

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO 33

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  27

#elif defined(ESP32)
    // Generic pinning for ESP32 development boards
    #define PIN_TRANSCEIVER_CS   27

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  21

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO 33

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  32
    
#elif defined(ESP8266)
    // Generic pinning for ESP8266 development boards (e.g. LOLIN/WEMOS D1 mini)
    #define PIN_TRANSCEIVER_CS   15

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  4

    // CC1101: GDO2 / RFM95W/SX127x: G1
    #define PIN_TRANSCEIVER_GPIO 5

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  2
    
#elif defined(ARDUINO_AVR_FEATHER32U4)
    // Pinning for Adafruit Feather 32u4 
    #define PIN_TRANSCEIVER_CS   8

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  7

    // CC1101: GDO2 / RFM95W/SX127x: G1 (not used)
    #define PIN_TRANSCEIVER_GPIO 99

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  4

#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
    // Use pinning for Adafruit Feather RP2040 with RFM95W "FeatherWing" ADA3232
    #define PIN_TRANSCEIVER_CS   7

    // CC1101: GDO0 / RFM95W/SX127x: G0
    #define PIN_TRANSCEIVER_IRQ  8

    // CC1101: GDO2 / RFM95W/SX127x: G1 (not used)
    #define PIN_TRANSCEIVER_GPIO 10

    // RFM95W/SX127x - GPIOxx / CC1101 - RADIOLIB_NC
    #define PIN_TRANSCEIVER_RST  11

#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#pragma message("Transmitter chip: " TRANSCEIVER_CHIP)
#pragma message("Pin config: RST->" STR(PIN_TRANSCEIVER_RST) ", CS->" STR(PIN_TRANSCEIVER_CS) ", GD0/G0/IRQ->" STR(PIN_TRANSCEIVER_IRQ) ", GDO2/G1/GPIO->" STR(PIN_TRANSCEIVER_GPIO) )

#endif
