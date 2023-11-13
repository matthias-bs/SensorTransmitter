///////////////////////////////////////////////////////////////////////////////////////////////////
// SensorTransmitter.ino
//
// Bresser 5-in-1/6-in-1/7-in-1 868 MHz Sensor Radio Transmitter
// based on CC1101 or SX1276/RFM95W and ESP32/ESP8266
//
// This can be used to emulate sensors for testing purposes or to implement sensors currently not
// available. In the the latter, emulate a sensor supported by the base station, but send
// measurement values. E.g. emulating a temperature sensor, the snow depth could be displayed by
// the base station.
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
//          Added checksum calculation
// 20231112 Added utilization of class WeatherSensor
//          Added JSON string as payload source
// 20231113 Added JSON string input from serial console
//          Added encodeBresserLightningPayload (DATA_RAW, DATA_GEN)
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "SensorTransmitter.h"
#include <RadioLib.h>
#include "logging.h"
#include "WeatherSensor.h"
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson

// SX1276 has the following connections:
// NSS pin:   PIN_TRANSCEIVER_CS
// DIO0 pin:  PIN_TRANSCEIVER_IRQ
// RESET pin: PIN_TRANSCEIVER_RST
// DIO1 pin:  PIN_TRANSCEIVER_GPIO
SX1276 radio = new Module(PIN_TRANSCEIVER_CS, PIN_TRANSCEIVER_IRQ, PIN_TRANSCEIVER_RST, PIN_TRANSCEIVER_GPIO);

void setup()
{
  Serial.begin(115200);

  // initialize SX1276
  log_i("[SX1276] Initializing ... ");
  // carrier frequency:                   868.3 MHz
  // bit rate:                            8.22 kbps
  // frequency deviation:                 57.136417 kHz
  // Rx bandwidth:                        270.0 kHz (CC1101) / 250 kHz (SX1276)
  // output power:                        10 dBm
  // preamble length:                     40 bits
  // Preamble: AA AA AA AA AA
  // Sync: 2D D4
  int state = radio.beginFSK(868.3, 8.21, 57.136417, 250, 10, 32);
  if (state == RADIOLIB_ERR_NONE)
  {
    log_i("success!");
  }
  else
  {
    log_e("failed, code %d", state);
    while (true)
      ;
  }

  // some modules have an external RF switch
  // controlled via two pins (RX enable, TX enable)
  // to enable automatic control of the switch,
  // call the following method
  // RX enable:   4
  // TX enable:   5
  /*
    radio.setRfSwitchPins(4, 5);
  */
}

// counter to keep track of transmitted packets
int count = 0;

WeatherSensor ws;

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_5in1.c (20220212)
//
// Example input data:
//   00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
//   EA EC 7F EB 5F EE EF FA FE 76 BB FA FF 15 13 80 14 A0 11 10 05 01 89 44 05 00
//   CC CC CC CC CC CC CC CC CC CC CC CC CC uu II sS GG DG WW  W TT  T HH RR RR Bt
// - C = Check, inverted data of 13 byte further
// - uu = checksum (number/count of set bits within bytes 14-25)
// - I = station ID (maybe)
// - G = wind gust in 1/10 m/s, normal binary coded, GGxG = 0x76D1 => 0x0176 = 256 + 118 = 374 => 37.4 m/s.  MSB is out of sequence.
// - D = wind direction 0..F = N..NNE..E..S..W..NNW
// - W = wind speed in 1/10 m/s, BCD coded, WWxW = 0x7512 => 0x0275 = 275 => 27.5 m/s. MSB is out of sequence.
// - T = temperature in 1/10 °C, BCD coded, TTxT = 1203 => 31.2 °C
// - t = temperature sign, minus if unequal 0
// - H = humidity in percent, BCD coded, HH = 23 => 23 %
// - R = rain in mm, BCD coded, RRRR = 1203 => 031.2 mm
// - B = Battery. 0=Ok, 8=Low.
// - s = startup, 0 after power-on/reset / 8 after 1 hour
// - S = sensor type, only low nibble used, 0x9 for Bresser Professional Rain Gauge
uint8_t encodeBresser5In1Payload(String msg_str, uint8_t *msg)
{
  uint8_t preamble[] = {0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t syncword[] = {0x2D, 0xD4};

  char buf[7];
  memcpy(msg, preamble, 4);
  memcpy(&msg[4], syncword, 2);

#if defined(DATA_RAW)
  uint8_t payload[] = {0xEA, 0xEC, 0x7F, 0xEB, 0x5F, 0xEE, 0xEF, 0xFA, 0xFE, 0x76, 0xBB, 0xFA, 0xFF,
                       0x15, 0x13, 0x80, 0x14, 0xA0, 0x11, 0x10, 0x05, 0x01, 0x89, 0x44, 0x05, 0x00};

#elif defined(DATA_GEN)
  uint8_t payload[26];
  ws.genMessage(0 /* slot */, 0xff /* id */, SENSOR_TYPE_WEATHER0 /* s_type */);

#elif defined(DATA_JSON_CONST)
  uint8_t payload[26];
  StaticJsonDocument<512> doc;
  const char json[] =
      "{\"sensor_id\":255,\"s_type\":1,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"temp_c\":12.3,\
        \"humidity\":44,\"wind_gust_meter_sec\":3.3,\"wind_avg_meter_sec\":2.2,\"wind_direction_deg\":111.1,\
        \"rain_mm\":123.4}";

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);

#elif defined(DATA_JSON_INPUT)
  if (!msg_str.length())
  {
    return 0;
  }

  uint8_t payload[26];
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, msg_str.c_str());

#endif

#if defined(DATA_JSON_INPUT) || defined(DATA_JSON_CONST)
  // Test if parsing succeeded
  if (error)
  {
    log_e("DeserializeJson() failed: %s", error.f_str());
    return 0;
  }

  ws.sensor[0].sensor_id = doc["sensor_id"];
  ws.sensor[0].s_type = doc["s_type"];
  ws.sensor[0].chan = doc["chan"];
  ws.sensor[0].startup = doc["startup"];
  ws.sensor[0].battery_ok = doc["battery_ok"];
  ws.sensor[0].w.temp_c = doc["temp_c"];
  ws.sensor[0].w.humidity = doc["humidity"];
  ws.sensor[0].w.wind_gust_meter_sec = doc["wind_gust_meter_sec"];
  ws.sensor[0].w.wind_avg_meter_sec = doc["wind_avg_meter_sec"];
  ws.sensor[0].w.wind_direction_deg = doc["wind_direction_deg"];
  ws.sensor[0].w.rain_mm = doc["rain_mm"];
#endif

#if !defined(DATA_RAW)
  payload[14] = (uint8_t)(ws.sensor[0].sensor_id & 0xFF);
  payload[15] = ((ws.sensor[0].startup ? 0 : 8) << 4) | ws.sensor[0].s_type;

  uint16_t wind = ws.sensor[0].w.wind_gust_meter_sec * 10;
  payload[16] = wind & 0xFF;
  payload[17] = (wind >> 8) & 0xF;

  uint8_t wdir = ws.sensor[0].w.wind_direction_deg / 22.5f;
  payload[17] |= wdir << 4;

  snprintf(buf, 7, "%04.1f", ws.sensor[0].w.wind_avg_meter_sec);
  payload[18] = ((buf[1] - '0') << 4) | (buf[3] - '0');
  payload[19] = buf[0] - '0';

  float temp_c = ws.sensor[0].w.temp_c;
  if (temp_c < 0)
  {
    temp_c *= -1;
    payload[25] = 1;
  }
  else
  {
    payload[25] = 0;
  }

  snprintf(buf, 7, "%04.1f", temp_c);
  payload[20] = ((buf[1] - '0') << 4) | (buf[3] - '0');
  payload[21] = buf[0] - '0';

  snprintf(buf, 7, "%02d", ws.sensor[0].w.humidity);
  payload[22] = ((buf[0] - '0') << 4) | (buf[1] - '0');

  snprintf(buf, 7, "%05.1f", ws.sensor[0].w.rain_mm);
  payload[23] = ((buf[2] - '0') << 4) | (buf[4] - '0');
  payload[24] = ((buf[0] - '0') << 4) | (buf[1] - '0');

  payload[25] |= (ws.sensor[0].battery_ok ? 0 : 8) << 4;

  // Calculate checksum (number number bits set in bytes 14-25)
  uint8_t bitsSet = 0;

  for (uint8_t p = 14; p < 26; p++)
  {
    uint8_t currentByte = payload[p];
    while (currentByte)
    {
      bitsSet += (currentByte & 1);
      currentByte >>= 1;
    }
  }
  payload[13] = bitsSet;
  log_d("Bits set: 0x%02X", bitsSet);

  // First 13 bytes are inverse of last 13 bytes
  for (unsigned col = 0; col < 26 / 2; ++col)
  {
    payload[col] = ~payload[col + 13];
  }
#endif

  memcpy(&msg[6], payload, 26);

  // Return message size
  return 4 + 2 + 26;
}


/**
Decoder for Bresser Lightning, outdoor sensor.

https://github.com/merbanan/rtl_433/issues/2140

DIGEST:8h8h ID:8h8h CTR:12h   ?4h8h KM:8d ?8h8h
       0 1     2 3      4 5h   5l 6    7   8 9

Preamble:

  aa 2d d4

Observed length depends on reset_limit.
The data has a whitening of 0xaa.


First two bytes are an LFSR-16 digest, generator 0x8810 key 0xabf9 with a final xor 0x899e
*/
uint8_t encodeBresserLightningPayload(String msg_str, uint8_t *msg)
{
  uint8_t preamble[] = {0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t syncword[] = {0x2D, 0xD4};

  char buf[7];
  memcpy(msg, preamble, 4);
  memcpy(&msg[4], syncword, 2);

#if defined(DATA_RAW)
  uint8_t payload[] = {0x73, 0x69, 0xB5, 0x08, 0xAA, 0xA2, 0x90, 0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#elif defined(DATA_GEN)
  uint8_t payload[26];
  ws.genMessage(0 /* slot */, 0xffff /* id */, SENSOR_TYPE_LIGHTNING /* s_type */);
#endif

#if !defined(DATA_RAW)

  payload[2] = (ws.sensor[0].sensor_id >> 8) & 0xFF;
  payload[3] = ws.sensor[0].sensor_id & 0xFF;

  payload[4] = ws.sensor[0].lgt.strike_count >> 4;
  payload[5] = ws.sensor[0].lgt.strike_count << 4;

  if (!ws.sensor[0].battery_ok) {
    payload[5] |= 8;
  }
  payload[5] ^= 0xA;

  payload[6] = (SENSOR_TYPE_LIGHTNING << 4);
  
  if (!ws.sensor[0].startup) {
    payload[6] |= 8;
  }
  payload[6] ^= 0xAA;

  payload[7] = ws.sensor[0].lgt.distance_km;

  payload[8] = 0;
  payload[9] = 0;
#endif

  int crc = crc16(&payload[2], 7, 0x1021 /* polynomial */, 0 /* init */);
  log_i("CRC: %0x", crc);
  crc ^= 0x899e;

  payload[0] = ((crc >> 8) & 0xFF);
  payload[1] = crc & 0xFF;

  for (int i=0; i<10; i++) {
    payload[i] ^= 0xAA;
  }

  memcpy(&msg[6], payload, 10);

  // Return message size
  return 4 + 2 + 10;
}

void loop()
{
  static String input_str;
  if (Serial.available())
  {
    input_str = Serial.readStringUntil('\n');
  }

  uint8_t msg_buf[40];

  //uint8_t msg_size = encodeBresser5In1Payload(input_str, msg_buf);
  uint8_t msg_size = encodeBresserLightningPayload(input_str, msg_buf);

  log_i("[SX1276] Transmitting packet (%d bytes)... ", msg_size);
  int state = radio.transmit(msg_buf, msg_size);

  if (state == RADIOLIB_ERR_NONE)
  {
    // the packet was successfully transmitted
    log_i(" success!");

    // print measured data rate
    log_i("[SX1276] Datarate:\t%f bps", radio.getDataRate());
  }
  else if (state == RADIOLIB_ERR_PACKET_TOO_LONG)
  {
    // the supplied packet was longer than 256 bytes
    log_e("too long!");
  }
  else if (state == RADIOLIB_ERR_TX_TIMEOUT)
  {
    // timeout occurred while transmitting packet
    log_e("timeout!");
  }
  else
  {
    // some other error occurred
    log_e("failed, code %d", state);
  }

  // wait for TX_INTERVAL seconds before transmitting again
  delay(TX_INTERVAL * 1000);
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes, uint16_t gen, uint16_t key)
{
    uint16_t sum = 0;
    for (unsigned k = 0; k < bytes; ++k) {
        uint8_t data = message[k];
        for (int i = 7; i >= 0; --i) {
            // fprintf(stderr, "key at bit %d : %04x\n", i, key);
            // if data bit is set then xor with key
            if ((data >> i) & 1)
                sum ^= key;

            // roll the key right (actually the lsb is dropped here)
            // and apply the gen (needs to include the dropped lsb as msb)
            if (key & 1)
                key = (key >> 1) ^ gen;
            else
                key = (key >> 1);
        }
    }
    return sum;
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
uint16_t crc16(uint8_t const message[], unsigned nBytes, uint16_t polynomial, uint16_t init)
{
    uint16_t remainder = init;
    unsigned byte, bit;

    for (byte = 0; byte < nBytes; ++byte) {
        remainder ^= message[byte] << 8;
        for (bit = 0; bit < 8; ++bit) {
            if (remainder & 0x8000) {
                remainder = (remainder << 1) ^ polynomial;
            }
            else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}
