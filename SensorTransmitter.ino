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
// 20231114 Added setting of encoder and tx_interval
// 20231115 Added support of CC1101 transceiver
//          Added encodeBresser<6In1|7In1|Leakage>Payload() - only raw data input!
// 20231117 Implemented encodeBresser6In1Payload() (basic functionality)
// 20231118 encodeBresser6In1Payload(): Added UV index and remaining (known) sensors
// 20231119 Restructured data generation and encoding
// 20231120 Implemented encodeBresser7In1Payload()
// 20231121 Implemented encodeBresserLeakage() - CRC errors at receiver
// 20240129 Fixed lightning counter encoding
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

#if defined(USE_CC1101)
CC1101 radio = new Module(PIN_TRANSCEIVER_CS, PIN_TRANSCEIVER_IRQ, RADIOLIB_NC, PIN_TRANSCEIVER_GPIO);
#endif
#if defined(USE_SX1276)
// SX1276 has the following connections:
// NSS pin:   PIN_TRANSCEIVER_CS
// DIO0 pin:  PIN_TRANSCEIVER_IRQ
// RESET pin: PIN_TRANSCEIVER_RST
// DIO1 pin:  PIN_TRANSCEIVER_GPIO
SX1276 radio = new Module(PIN_TRANSCEIVER_CS, PIN_TRANSCEIVER_IRQ, PIN_TRANSCEIVER_RST, PIN_TRANSCEIVER_GPIO);
#endif

void setup()
{
  Serial.begin(115200);

  // initialize SX1276
  log_i("%s Initializing ... ", TRANSCEIVER_CHIP);
// carrier frequency:                   868.3 MHz
// bit rate:                            8.22 kbps
// frequency deviation:                 57.136417 kHz
// Rx bandwidth:                        270.0 kHz (CC1101) / 250 kHz (SX1276)
// output power:                        10 dBm
// preamble length:                     40 bits
// Preamble: AA AA AA AA AA
// Sync: 2D D4
#ifdef USE_CC1101
  int state = radio.begin(868.3, 8.21, 57.136417, 270, 10, 32);
#else
  int state = radio.beginFSK(868.3, 8.21, 57.136417, 250, 10, 32);
#endif
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

int msgBegin(uint8_t *msg)
{
  uint8_t preamble[] = {0xAA, 0xAA, 0xAA, 0xAA};
  uint8_t syncword[] = {0x2D, 0xD4};

  memcpy(msg, preamble, sizeof(preamble));
  memcpy(&msg[sizeof(preamble)], syncword, sizeof(syncword));

  return sizeof(preamble) + sizeof(syncword);
}

#if defined(DATA_RAW)
uint8_t rawPayload(Encoders encoder, uint8_t *msg)
{
  uint8_t payload_5in1[] = {0xEA, 0xEC, 0x7F, 0xEB, 0x5F, 0xEE, 0xEF, 0xFA, 0xFE, 0x76, 0xBB, 0xFA, 0xFF,
                            0x15, 0x13, 0x80, 0x14, 0xA0, 0x11, 0x10, 0x05, 0x01, 0x89, 0x44, 0x05, 0x00};
  uint8_t payload_6in1[] = {0x2A, 0xAF, 0x21, 0x10, 0x34, 0x27, 0x18, 0xFF, 0xAA, 0xFF, 0x29, 0x28, 0xFF,
                            0xBB, 0x89, 0xFF, 0x01, 0x1F};
  uint8_t payload_7in1[] = {0xC4, 0xD6, 0x3A, 0xC5, 0xBD, 0xFA, 0x18, 0xAA, 0xAA, 0xAA, 0xAA, 0xAB, 0xFC,
                            0xAA, 0x98, 0xDA, 0x89, 0xA3, 0x2F, 0xEC, 0xAF, 0x9A, 0xAA, 0xAA, 0xAA, 0x00};
  uint8_t payload_lightning[] = {0x73, 0x69, 0xB5, 0x08, 0xAA, 0xA2, 0x90, 0xAA, 0xAA, 0xAA, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t payload_leakage[] = {0xB3, 0xDA, 0x55, 0x57, 0x17, 0x40, 0x53, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB};
  if (encoder == Encoders::ENC_BRESSER_5IN1)
  {
    memcpy(msg, payload_5in1, 26);
    return 26;
  }
  else if (encoder == Encoders::ENC_BRESSER_6IN1)
  {
    memcpy(msg, payload_6in1, 18);
    return 18;
  }
  else if (encoder == Encoders::ENC_BRESSER_7IN1)
  {
    memcpy(msg, payload_7in1, 26);
    return 26;
  }
  else if (encoder == Encoders::ENC_BRESSER_LIGHTNING)
  {
    memcpy(msg, payload_lightning, 26);
    return 26;
  }
  else if (encoder == Encoders::ENC_BRESSER_LEAKAGE)
  {
    memcpy(msg, payload_leakage, 26);
    return 26;
  }
  else
  {
    log_e("Encoder not supported!");
    return 0;
  }
}
#endif

#if defined(DATA_GEN)
void genData(Encoders encoder)
{
  if (encoder == Encoders::ENC_BRESSER_5IN1)
  {
    ws.genMessage(0 /* slot */, 0xff /* id */, SENSOR_TYPE_WEATHER0 /* s_type */);
  }
  else if (encoder == Encoders::ENC_BRESSER_6IN1)
  {
    ws.genMessage(0 /* slot */, 0xFFFFFFFF /* id */, SENSOR_TYPE_WEATHER1 /* s_type */);
  }
  else if (encoder == Encoders::ENC_BRESSER_7IN1)
  {
    ws.genMessage(0 /* slot */, 0xFFFF /* id */, SENSOR_TYPE_WEATHER1 /* s_type */);
  }
  else if (encoder == Encoders::ENC_BRESSER_LIGHTNING)
  {
    ws.genMessage(0 /* slot */, 0xFFFF /* id */, SENSOR_TYPE_LIGHTNING /* s_type */);
  }
  else if (encoder == Encoders::ENC_BRESSER_LEAKAGE)
  {
    ws.genMessage(0 /* slot */, 0xFFFFFFFF /* id */, SENSOR_TYPE_LEAKAGE /* s_type */);
  }
  else
  {
    log_e("Encoder not supported!");
  }
}
#endif

#if defined(DATA_JSON_CONST)
void genJson(Encoders encoder, String &json_str)
{
  const String json_5in1 =
      "{\"sensor_id\":255,\"s_type\":1,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"temp_c\":12.3,\
      \"humidity\":44,\"wind_gust_meter_sec\":3.3,\"wind_avg_meter_sec\":2.2,\"wind_direction_deg\":111.1,\
      \"rain_mm\":123.4}";

  const String json_6in1 =
      "{\"sensor_id\":4294967295,\"s_type\":1,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"temp_c\":12.3,\
      \"humidity\":44,\"wind_gust_meter_sec\":3.3,\"wind_avg_meter_sec\":2.2,\"wind_direction_deg\":111.1,\
      \"rain_mm\":12345.6,\"uv\":7.8}";

  const String json_7in1 =
      "{\"sensor_id\":65535,\"s_type\":1,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"temp_c\":12.3,\
      \"humidity\":44,\"wind_gust_meter_sec\":3.3,\"wind_avg_meter_sec\":2.2,\"wind_direction_deg\":111.1,\
      \"rain_mm\":12345.6}";

  const String json_lightning =
      "{\"sensor_id\":65535,\"s_type\":9,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"strike_count\":22,\
      \"distance_km\":44}";

  const String json_leakage = "{\"sensor_id\":4294967295,\"s_type\":5,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"alarm\":1}";

  if (encoder == Encoders::ENC_BRESSER_5IN1)
  {
    json_str = json_5in1;
  }
  else if (encoder == Encoders::ENC_BRESSER_6IN1)
  {
    json_str = json_6in1;
  }
  else if (encoder == Encoders::ENC_BRESSER_7IN1)
  {
    json_str = json_7in1;
  }
  else if (encoder == Encoders::ENC_BRESSER_LIGHTNING)
  {
    json_str = json_lightning;
  }
  else if (encoder == Encoders::ENC_BRESSER_LEAKAGE)
  {
    json_str = json_leakage;
  }
  else
  {
    log_e("Encoder not supported!");
  }
}
#endif

#if defined(DATA_JSON_INPUT) || defined(DATA_JSON_CONST)
bool deSerialize(Encoders encoder, String json_str)
{
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json_str.c_str());

  // Test if parsing succeeded
  if (error)
  {
    log_e("DeserializeJson() failed: %s", error.f_str());
    return false;
  }

  ws.sensor[0].sensor_id = doc["sensor_id"];
  ws.sensor[0].s_type = doc["s_type"];
  ws.sensor[0].chan = doc["chan"];
  ws.sensor[0].startup = doc["startup"];
  ws.sensor[0].battery_ok = doc["battery_ok"];

  if (encoder == Encoders::ENC_BRESSER_5IN1)
  {
    ws.sensor[0].w.temp_c = doc["temp_c"];
    ws.sensor[0].w.humidity = doc["humidity"];
    ws.sensor[0].w.wind_gust_meter_sec = doc["wind_gust_meter_sec"];
    ws.sensor[0].w.wind_avg_meter_sec = doc["wind_avg_meter_sec"];
    ws.sensor[0].w.wind_direction_deg = doc["wind_direction_deg"];
    ws.sensor[0].w.rain_mm = doc["rain_mm"];
  }
  else if (encoder == Encoders::ENC_BRESSER_6IN1)
  {
    if (ws.sensor[0].s_type == SENSOR_TYPE_SOIL)
    {
      ws.sensor[0].soil.temp_c = doc["temp_c"];
      ws.sensor[0].soil.moisture = doc["moisture"];
    }
    else
    {
      ws.sensor[0].w.temp_c = doc["temp_c"];
      ws.sensor[0].w.humidity = doc["humidity"];
      ws.sensor[0].w.wind_gust_meter_sec = doc["wind_gust_meter_sec"];
      ws.sensor[0].w.wind_avg_meter_sec = doc["wind_avg_meter_sec"];
      ws.sensor[0].w.wind_direction_deg = doc["wind_direction_deg"];
      ws.sensor[0].w.rain_mm = doc["rain_mm"];
      ws.sensor[0].w.uv = doc["uv"];
    }
  }
  else if (encoder == Encoders::ENC_BRESSER_7IN1)
  {
    if (ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1)
    {
      ws.sensor[0].w.temp_c = doc["temp_c"];
      ws.sensor[0].w.humidity = doc["humidity"];
      ws.sensor[0].w.wind_gust_meter_sec = doc["wind_gust_meter_sec"];
      ws.sensor[0].w.wind_avg_meter_sec = doc["wind_avg_meter_sec"];
      ws.sensor[0].w.wind_direction_deg = doc["wind_direction_deg"];
      ws.sensor[0].w.rain_mm = doc["rain_mm"];
      ws.sensor[0].w.uv = doc["uv"];
      ws.sensor[0].w.light_klx = doc["light_klx"];
    }
    else if (ws.sensor[0].s_type == SENSOR_TYPE_AIR_PM)
    {
      ws.sensor[0].pm.pm_2_5 = doc["pm_2_5"];
      ws.sensor[0].pm.pm_10 = doc["pm_10"];
    }
  }
  else if (encoder == Encoders::ENC_BRESSER_LIGHTNING)
  {
    ws.sensor[0].lgt.strike_count = doc["strike_count"];
    ws.sensor[0].lgt.distance_km = doc["distance_km"];
  }
  else if (encoder == Encoders::ENC_BRESSER_LEAKAGE)
  {
    ws.sensor[0].leak.alarm = doc["alarm"];
  }
  else
  {
    log_e("Encoder not supported!");
    return false;
  }
  return true;
}
#endif

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
uint8_t encodeBresser5In1Payload(uint8_t *msg)
{
  uint8_t payload[26] = {0};
  char buf[7];

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

  memcpy(msg, payload, 26);

  // Return message size
  return 26;
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_6in1.c (20220608)
//
// - also Bresser Weather Center 7-in-1 indoor sensor.
// - also Bresser new 5-in-1 sensors.
// - also Froggit WH6000 sensors.
// - also rebranded as Ventus C8488A (W835)
// - also Bresser 3-in-1 Professional Wind Gauge / Anemometer PN 7002531
// - also Bresser Pool / Spa Thermometer PN 7009973 (s_type = 3)
//
// There are at least two different message types:
// - 24 seconds interval for temperature, hum, uv and rain (alternating messages)
// - 12 seconds interval for wind data (every message)
//
// Also Bresser Explore Scientific SM60020 Soil moisture Sensor.
// https://www.bresser.de/en/Weather-Time/Accessories/EXPLORE-SCIENTIFIC-Soil-Moisture-and-Soil-Temperature-Sensor.html
//
// Moisture:
//
//     f16e 187000e34 7 ffffff0000 252 2 16 fff 004 000 [25,2, 99%, CH 7]
//     DIGEST:8h8h ID?8h8h8h8h TYPE:4h STARTUP:1b CH:3d 8h 8h8h 8h8h TEMP:12h ?2b BATT:1b ?1b MOIST:8h UV?~12h ?4h CHKSUM:8h
//
// Moisture is transmitted in the humidity field as index 1-16: 0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 99.
// The Wind speed and direction fields decode to valid zero but we exclude them from the output.
//
//     aaaa2dd4e3ae1870079341ffffff0000221201fff279 [Batt ok]
//     aaaa2dd43d2c1870079341ffffff0000219001fff2fc [Batt low]
//
//     {206}55555555545ba83e803100058631ff11fe6611ffffffff01cc00 [Hum 96% Temp 3.8 C Wind 0.7 m/s]
//     {205}55555555545ba999263100058631fffffe66d006092bffe0cff8 [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     {199}55555555545ba840523100058631ff77fe668000495fff0bbe [Hum 95% Temp 3.0 C Wind 0.4 m/s]
//     {205}55555555545ba94d063100058631fffffe665006092bffe14ff8
//     {206}55555555545ba860703100058631fffffe6651ffffffff0135fc [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     {205}55555555545ba924d23100058631ff99fe68b004e92dffe073f8 [Hum 96% Temp 2.7 C Wind 0.4 m/s]
//     {202}55555555545ba813403100058631ff77fe6810050929ffe1180 [Hum 94% Temp 2.8 C Wind 0.4 m/s]
//     {205}55555555545ba98be83100058631fffffe6130050929ffe17800 [Hum 95% Temp 2.8 C Wind 0.8 m/s]
//
//     2dd4  1f 40 18 80 02 c3 18 ff 88 ff 33 08 ff ff ff ff 80 e6 00 [Hum 96% Temp 3.8 C Wind 0.7 m/s]
//     2dd4  cc 93 18 80 02 c3 18 ff ff ff 33 68 03 04 95 ff f0 67 3f [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     2dd4  20 29 18 80 02 c3 18 ff bb ff 33 40 00 24 af ff 85 df    [Hum 95% Temp 3.0 C Wind 0.4 m/s]
//     2dd4  a6 83 18 80 02 c3 18 ff ff ff 33 28 03 04 95 ff f0 a7 3f
//     2dd4  30 38 18 80 02 c3 18 ff ff ff 33 28 ff ff ff ff 80 9a 7f [Hum 95% Temp 3.0 C Wind 0.0 m/s]
//     2dd4  92 69 18 80 02 c3 18 ff cc ff 34 58 02 74 96 ff f0 39 3f [Hum 96% Temp 2.7 C Wind 0.4 m/s]
//     2dd4  09 a0 18 80 02 c3 18 ff bb ff 34 08 02 84 94 ff f0 8c 0  [Hum 94% Temp 2.8 C Wind 0.4 m/s]
//     2dd4  c5 f4 18 80 02 c3 18 ff ff ff 30 98 02 84 94 ff f0 bc 00 [Hum 95% Temp 2.8 C Wind 0.8 m/s]
//
//     {147} 5e aa 18 80 02 c3 18 fa 8f fb 27 68 11 84 81 ff f0 72 00 [Temp 11.8 C  Hum 81%]
//     {149} ae d1 18 80 02 c3 18 fa 8d fb 26 78 ff ff ff fe 02 db f0
//     {150} f8 2e 18 80 02 c3 18 fc c6 fd 26 38 11 84 81 ff f0 68 00 [Temp 11.8 C  Hum 81%]
//     {149} c4 7d 18 80 02 c3 18 fc 78 fd 29 28 ff ff ff fe 03 97 f0
//     {149} 28 1e 18 80 02 c3 18 fb b7 fc 26 58 ff ff ff fe 02 c3 f0
//     {150} 21 e8 18 80 02 c3 18 fb 9c fc 33 08 11 84 81 ff f0 b7 f8 [Temp 11.8 C  Hum 81%]
//     {149} 83 ae 18 80 02 c3 18 fc 78 fc 29 28 ff ff ff fe 03 98 00
//     {150} 5c e4 18 80 02 c3 18 fb ba fc 26 98 11 84 81 ff f0 16 00 [Temp 11.8 C  Hum 81%]
//     {148} d0 bd 18 80 02 c3 18 f9 ad fa 26 48 ff ff ff fe 02 ff f0
//
// Wind and Temperature/Humidity or Rain:
//
//     DIGEST:8h8h ID:8h8h8h8h TYPE:4h STARTUP:1b CH:3d WSPEED:~8h~4h ~4h~8h WDIR:12h ?4h TEMP:8h.4h ?2b BATT:1b ?1b HUM:8h UV?~12h ?4h CHKSUM:8h
//     DIGEST:8h8h ID:8h8h8h8h TYPE:4h STARTUP:1b CH:3d WSPEED:~8h~4h ~4h~8h WDIR:12h ?4h RAINFLAG:8h RAIN:8h8h UV:8h8h CHKSUM:8h
//
// Digest is LFSR-16 gen 0x8810 key 0x5412, excluding the add-checksum and trailer.
// Checksum is 8-bit add (with carry) to 0xff.
//
// Notes on different sensors:
//
// - 1910 084d 18 : RebeckaJohansson, VENTUS W835
// - 2030 088d 10 : mvdgrift, Wi-Fi Colour Weather Station with 5in1 Sensor, Art.No.: 7002580, ff 01 in the UV field is (obviously) invalid.
// - 1970 0d57 18 : danrhjones, bresser 5-in-1 model 7002580, no UV
// - 18b0 0301 18 : konserninjohtaja 6-in-1 outdoor sensor
// - 18c0 0f10 18 : rege245 BRESSER-PC-Weather-station-with-6-in-1-outdoor-sensor
// - 1880 02c3 18 : f4gqk 6-in-1
// - 18b0 0887 18 : npkap
uint8_t encodeBresser6In1Payload(uint8_t *msg)
{
  static int msg_type;
  char buf[8];
  uint8_t payload[18] = {0};

  payload[2] = ws.sensor[0].sensor_id >> 24;
  payload[3] = (ws.sensor[0].sensor_id >> 16) & 0xFF;
  payload[4] = (ws.sensor[0].sensor_id >> 8) & 0xFF;
  payload[5] = (ws.sensor[0].sensor_id) & 0xFF;
  payload[6] = ws.sensor[0].s_type << 4;
  payload[6] |= (ws.sensor[0].startup ? 0 : 8) | ws.sensor[0].chan;

  snprintf(buf, 7, "%04.1f", ws.sensor[0].w.wind_gust_meter_sec);
  log_d("Wind gust: %04.1f", ws.sensor[0].w.wind_gust_meter_sec);
  payload[7] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[8] = (buf[3] - '0') << 4;

  snprintf(buf, 7, "%04.1f", ws.sensor[0].w.wind_avg_meter_sec);
  log_d("Wind avg: %04.1f", ws.sensor[0].w.wind_avg_meter_sec);
  payload[9] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[8] |= buf[3] - '0';

  // Invert bytes
  payload[7] ^= 0xFF;
  payload[8] ^= 0xFF;
  payload[9] ^= 0xFF;

  snprintf(buf, 7, "%03d", (int)ws.sensor[0].w.wind_direction_deg);
  log_d("Wind dir: %03d", (int)ws.sensor[0].w.wind_direction_deg);
  payload[10] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[11] = (buf[2] - '0') << 4;

  if ((ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1) ||
      (ws.sensor[0].s_type == SENSOR_TYPE_POOL_THERMO) ||
      (ws.sensor[0].s_type == SENSOR_TYPE_THERMO_HYGRO) ||
      (ws.sensor[0].s_type == SENSOR_TYPE_SOIL))
  {
    if (msg_type == 0)
    {
      float temp_c;
      if (ws.sensor[0].s_type == SENSOR_TYPE_SOIL)
      {
        temp_c = ws.sensor[0].soil.temp_c;
      }
      else
      {
        temp_c = ws.sensor[0].w.temp_c;
      }
      log_d("Temp: %04.1f", temp_c);
      if (temp_c < 0)
      {
        temp_c += 100;
        payload[13] = 8;
      }
      else
      {
        payload[13] = 0;
      }

      snprintf(buf, 7, "%04.1f", temp_c);
      payload[12] = ((buf[0] - '0') << 4) | (buf[1] - '0');
      payload[13] |= ((buf[3] - '0') << 4) | (ws.sensor[0].battery_ok ? 2 : 0);
      payload[16] = 0; // Flags: temp_ok

      if ((ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1) ||
          (ws.sensor[0].s_type == SENSOR_TYPE_THERMO_HYGRO))
      {
        snprintf(buf, 7, "%02d", ws.sensor[0].w.humidity);
        payload[14] = ((buf[0] - '0') << 4) | (buf[1] - '0');
      }

      if (ws.sensor[0].s_type == SENSOR_TYPE_SOIL)
      {
        int const moisture_map[] = {0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 99}; // scale is 20/3
        for (int i = 0; i < 16; i++)
        {
          if (moisture_map[i] > ws.sensor[0].soil.moisture)
          {
            log_d("Moisture: %d Index: %d", ws.sensor[0].soil.moisture, i);
            payload[14] = i;
            break;
          }
        }
      }

      if (ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1)
      {
        msg_type = 1;
      }
    } // msg_type == 0
    else
    {
      snprintf(buf, 8, "%07.1f", ws.sensor[0].w.rain_mm);
      log_d("Rain: %07.1f", ws.sensor[0].w.rain_mm);
      payload[12] = ((buf[0] - '0') << 4) | (buf[1] - '0');
      payload[13] = ((buf[2] - '0') << 4) | (buf[3] - '0');
      payload[14] = ((buf[4] - '0') << 4) | (buf[6] - '0');
      payload[12] ^= 0xFF;
      payload[13] ^= 0xFF;
      payload[14] ^= 0xFF;
      payload[16] = 1; // Flags: !temp_ok
      msg_type = 0;
    }
  }

  snprintf(buf, 8, "%04.1f", ws.sensor[0].w.uv);
  log_d("UV: %04.1f", ws.sensor[0].w.uv);
  payload[15] = ((buf[0] - '0') << 4) | (buf[1] - '0');
  payload[16] |= ((buf[3] - '0') << 4);
  payload[15] ^= 0xFF;
  payload[16] ^= 0xF0;

  int sum = add_bytes(&payload[2], 15);
  int chk = 0xFF - (sum & 0xFF);
  log_d("Checksum: 0x%02X vs 0x%02X", chk, payload[17]);
  payload[17] = chk;

  // int crc = crc16(&payload[2], 16, 0x1021 /* polynomial */, 0 /* init */);
  // int digest = crc ^ 0xE359;
  //  log_d("CRC: 0x%04X", crc ^ 0xE359);
  int digest = lfsr_digest16(&payload[2], 15, 0x8810, 0x5412);
  payload[0] = digest >> 8;
  payload[1] = digest & 0xFF;

  memcpy(msg, payload, 18);

  // Return message size
  return 18;
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_7in1.c (20230215)
//
/**
Decoder for Bresser Weather Center 7-in-1, outdoor sensor.
See https://github.com/merbanan/rtl_433/issues/1492
Preamble:
    aa aa aa aa aa 2d d4
Observed length depends on reset_limit.
The data has a whitening of 0xaa.

Weather Center
Data layout:
    {271}631d05c09e9a18abaabaaaaaaaaa8adacbacff9cafcaaaaaaa000000000000000000
    {262}10b8b4a5a3ca10aaaaaaaaaaaaaa8bcacbaaaa2aaaaaaaaaaa0000000000000000 [0.08 klx]
    {220}543bb4a5a3ca10aaaaaaaaaaaaaa8bcacbaaaa28aaaaaaaaaa00000 [0.08 klx]
    {273}2492b4a5a3ca10aaaaaaaaaaaaaa8bdacbaaaa2daaaaaaaaaa0000000000000000000 [0.08klx]
    {269}9a59b4a5a3da10aaaaaaaaaaaaaa8bdac8afea28a8caaaaaaa000000000000000000 [54.0 klx UV=2.6]
    {230}fe15b4a5a3da10aaaaaaaaaaaaaa8bdacbba382aacdaaaaaaa00000000 [109.2klx   UV=6.7]
    {254}2544b4a5a32a10aaaaaaaaaaaaaa8bdac88aaaaabeaaaaaaaa00000000000000 [200.000 klx UV=14
    DIGEST:8h8h ID?8h8h WDIR:8h4h 4h STYPE:4h STARTUP:1b CH:3d WGUST:8h.4h WAVG:8h.4h RAIN:8h8h4h.4h RAIN?:8h TEMP:8h.4hC FLAGS?:4h HUM:8h% LIGHT:8h4h,8h4hKL UV:8h.4h TRAILER:8h8h8h4h
Unit of light is kLux (not W/m²).

Air Quality Sensor PM2.5 / PM10 Sensor (PN 7009970)
Data layout:
DIGEST:8h8h ID?8h8h ?8h8h STYPE:4h STARTUP:1b CH:3b ?8h 4h ?4h8h4h PM_2_5:4h8h4h PM10:4h8h4h ?4h ?8h4h BATT:1b ?3b ?8h8h8h8h8h8h TRAILER:8h8h8h

STYPE, STARTUP and CH are not covered by whitening. Probably also ID.
First two bytes are an LFSR-16 digest, generator 0x8810 key 0xba95 with a final xor 0x6df1, which likely means we got that wrong.
*/
uint8_t encodeBresser7In1Payload(uint8_t *msg)
{
  char buf[8];
  uint8_t payload[26] = {0};

  payload[2] = (ws.sensor[0].sensor_id >> 8) & 0xFF;
  payload[3] = (ws.sensor[0].sensor_id) & 0xFF;
  payload[15] = (ws.sensor[0].battery_ok ? 0 : 4) ^ 0xAA;
  payload[6] = ws.sensor[0].s_type << 4;
  payload[6] |= (!ws.sensor[0].startup) << 3 | ws.sensor[0].chan;
  payload[6] ^= 0xAA;

  if (ws.sensor[0].s_type == SENSOR_TYPE_WEATHER1)
  {
    snprintf(buf, 7, "%03d", (int)ws.sensor[0].w.wind_direction_deg);
    log_d("Wind dir: %03d", (int)ws.sensor[0].w.wind_direction_deg);
    payload[4] = ((buf[0] - '0') << 4) | (buf[1] - '0');
    payload[5] = (buf[2] - '0') << 4;

    // payload[6] |= (ws.sensor[0].startup ? 0 : 8) | ws.sensor[0].chan;

    snprintf(buf, 7, "%04.1f", ws.sensor[0].w.wind_gust_meter_sec);
    log_d("Wind gust: %04.1f", ws.sensor[0].w.wind_gust_meter_sec);
    payload[7] = ((buf[0] - '0') << 4) | (buf[1] - '0');
    payload[8] = (buf[3] - '0') << 4;

    snprintf(buf, 7, "%04.1f", ws.sensor[0].w.wind_avg_meter_sec);
    log_d("Wind avg: %04.1f", ws.sensor[0].w.wind_avg_meter_sec);
    payload[9] = ((buf[1] - '0') << 4) | (buf[3] - '0');
    payload[8] |= buf[0] - '0';

    snprintf(buf, 8, "%07.1f", ws.sensor[0].w.rain_mm);
    log_d("Rain: %07.1f", ws.sensor[0].w.rain_mm);
    payload[10] = ((buf[0] - '0') << 4) | (buf[1] - '0');
    payload[11] = ((buf[2] - '0') << 4) | (buf[3] - '0');
    payload[12] = ((buf[4] - '0') << 4) | (buf[6] - '0');

    float temp_c = ws.sensor[0].w.temp_c;
    log_d("Temp: %04.1f", temp_c);
    if (temp_c < 0)
    {
      temp_c += 100;
    }
    snprintf(buf, 7, "%04.1f", temp_c);
    payload[14] = ((buf[0] - '0') << 4) | (buf[1] - '0');
    payload[15] |= ((buf[3] - '0') << 4);

    snprintf(buf, 7, "%02d", ws.sensor[0].w.humidity);
    payload[16] = ((buf[0] - '0') << 4) | (buf[1] - '0');

    snprintf(buf, 8, "%04.1f", ws.sensor[0].w.uv);
    log_d("UV: %04.1f", ws.sensor[0].w.uv);
    payload[20] = ((buf[0] - '0') << 4) | (buf[1] - '0');
    payload[21] |= ((buf[3] - '0') << 4);

    snprintf(buf, 8, "%06d", (int)(ws.sensor[0].w.light_klx * 1000));
    log_d("Light: %06d", (int)(ws.sensor[0].w.light_klx * 1000));
    payload[17] = ((buf[0] - '0') << 4) | (buf[1] - '0');
    payload[18] = ((buf[2] - '0') << 4) | (buf[3] - '0');
    payload[19] = ((buf[4] - '0') << 4) | (buf[5] - '0');
  }
  else if (ws.sensor[0].s_type == SENSOR_TYPE_AIR_PM)
  {
    snprintf(buf, 8, "%04d", ws.sensor[0].pm.pm_2_5);
    log_d("PM2.5: %04d", ws.sensor[0].pm.pm_2_5);
    payload[10] = (buf[0] - '0');
    payload[11] = ((buf[1] - '0') << 4) | (buf[2] - '0');
    payload[12] = ((buf[3] - '0') << 4);

    snprintf(buf, 8, "%04d", ws.sensor[0].pm.pm_10);
    log_d("PM10: %04d", ws.sensor[0].pm.pm_10);
    payload[12] = (buf[0] - '0');
    payload[13] = ((buf[1] - '0') << 4) | (buf[2] - '0');
    payload[14] = ((buf[3] - '0') << 4);
  }

  // LFSR-16 digest, generator 0x8810 key 0xba95 final xor 0x6df1
  // int chkdgst = (msgw[0] << 8) | msgw[1];
  // for (int i = 2; i < 26; i++)
  // {
  //   payload[i] ^= 0xAA;
  // }
  int digest = lfsr_digest16(&payload[2], 23, 0x8810, 0xba95);
  digest ^= 0x6df1;
  payload[0] = digest >> 8;
  payload[1] = digest & 0xFF;

  for (int i = 0; i < 26; i++)
  {
    payload[i] ^= 0xAA;
  }
  // log_d("Digest: 0x%04X", digest ^ 0xAAAA ^ 0x6df1);

  memcpy(msg, payload, 26);

  // Return message size
  return 26;
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
uint8_t encodeBresserLightningPayload(uint8_t *msg)
{
  uint8_t payload[10] = {0};
  char buf[5];

  payload[2] = (ws.sensor[0].sensor_id >> 8) & 0xFF;
  payload[3] = ws.sensor[0].sensor_id & 0xFF;

  // Counter encoded as BCD with most significant digit counting up to 15!
  snprintf(buf, 5, "%04d", ws.sensor[0].lgt.strike_count);
  log_d("count: %04d", ws.sensor[0].lgt.strike_count);
  payload[4] = ((ws.sensor[0].lgt.strike_count / 100) << 4) | (buf[2] - '0');
  payload[5] = (buf[3] - '0') << 4;

  if (!ws.sensor[0].battery_ok)
  {
    payload[5] |= 8;
  }
  payload[5] ^= 0xA;

  payload[6] = (SENSOR_TYPE_LIGHTNING << 4);

  if (!ws.sensor[0].startup)
  {
    payload[6] |= 8;
  }
  payload[6] ^= 0xAA;

  payload[7] = ws.sensor[0].lgt.distance_km;

  payload[8] = 0;
  payload[9] = 0;

  int crc = crc16(&payload[2], 7, 0x1021 /* polynomial */, 0 /* init */);
  log_d("CRC: 0x%04X", crc);
  crc ^= 0x899e;

  payload[0] = ((crc >> 8) & 0xFF);
  payload[1] = crc & 0xFF;

  for (int i = 0; i < 10; i++)
  {
    payload[i] ^= 0xAA;
  }

  memcpy(msg, payload, 10);

  // Return message size
  return 10;
}

/**
 * Decoder for Bresser Water Leakage outdoor sensor
 *
 * https://github.com/matthias-bs/BresserWeatherSensorReceiver/issues/77
 *
 * Preamble: aa aa 2d d4
 *
 * hhhh ID:hhhhhhhh TYPE:4d NSTARTUP:b CH:3d ALARM:b NALARM:b BATT:bb FLAGS:bbbb hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
 *
 * Examples:
 * ---------
 * [Bresser Water Leakage Sensor, PN 7009975]
 *
 *[00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25]
 *
 * C7 70 35 97 04 08 57 70 00 00 00 00 00 00 00 00 03 FF FF FF FF FF FF FF FF FF [CH7]
 * DF 7D 36 49 27 09 56 70 00 00 00 00 00 00 00 00 03 FF FF FF FF FF FF FF FF FF [CH6]
 * 9E 30 79 84 33 06 55 70 00 00 00 00 00 00 00 00 03 FF FD DF FF BF FF DF FF FF [CH5]
 * 37 D8 57 19 73 02 51 70 00 00 00 00 00 00 00 00 03 FF FF FF FF FF BF FF EF FB [set CH4, received CH1 -> switch not positioned correctly]
 * E2 C8 68 27 91 24 54 70 00 00 00 00 00 00 00 00 03 FF FF FF FF FF FF FF FF FF [CH4]
 * B3 DA 55 57 17 40 53 70 00 00 00 00 00 00 00 00 03 FF FF FF FF FF FF FF FF FB [CH3]
 * 37 FA 84 73 03 02 52 70 00 00 00 00 00 00 00 00 03 FF FF FF DF FF FF FF FF FF [CH2]
 * 27 F3 80 02 52 88 51 70 00 00 00 00 00 00 00 00 03 FF FF FF FF FF DF FF FF FF [CH1]
 * A6 FB 80 02 52 88 59 70 00 00 00 00 00 00 00 00 03 FD F7 FF FF BF FF FF FF FF [CH1+NSTARTUP]
 * A6 FB 80 02 52 88 59 B0 00 00 00 00 00 00 00 00 03 FF FF FF FD FF F7 FF FF FF [CH1+NSTARTUP+ALARM]
 * A6 FB 80 02 52 88 59 70 00 00 00 00 00 00 00 00 03 FF FF BF F7 F7 FD 7F FF FF [CH1+NSTARTUP]
 * [Reset]
 * C0 10 36 79 37 09 51 70 00 00 00 00 00 00 00 00 01 1E FD FD FF FF FF DF FF FF [CH1]
 * C0 10 36 79 37 09 51 B0 00 00 00 00 00 00 00 00 03 FE FD FF AF FF FF FF FF FD [CH1+ALARM]
 * [Reset]
 * 71 9C 54 81 72 09 51 40 00 00 00 00 00 00 00 00 0F FF FF FF FF FF FF DF FF FE [CH1+BATT_LO]
 * 71 9C 54 81 72 09 51 40 00 00 00 00 00 00 00 00 0F FE FF FF FF FF FB FF FF FF
 * 71 9C 54 81 72 09 51 40 00 00 00 00 00 00 00 00 07 FD F7 FF DF FF FF DF FF FF
 * 71 9C 54 81 72 09 51 80 00 00 00 00 00 00 00 00 1F FF FF F7 FF FF FF FF FF FF [CH1+BATT_LO+ALARM]
 * F0 94 54 81 72 09 59 40 00 00 00 00 00 00 00 00 0F FF DF FF FF FF FF BF FD F7 [CH1+BATT_LO+NSTARTUP]
 * F0 94 54 81 72 09 59 80 00 00 00 00 00 00 00 00 03 FF B7 FF ED FF FF FF DF FF [CH1+BATT_LO+NSTARTUP+ALARM]
 *
 * - The actual message length is not known (probably 16 or 17 bytes)
 * - The first two bytes are presumably a checksum/crc/digest; algorithm still to be found
 * - The ID changes on power-up/reset
 * - NSTARTUP changes from 0 to 1 approx. one hour after power-on/reset
 */
uint8_t encodeBresserLeakagePayload(uint8_t *msg)
{
  uint8_t payload[10] = {0x00};

  payload[2] = ws.sensor[0].sensor_id >> 24;
  payload[3] = (ws.sensor[0].sensor_id >> 16) & 0xFF;
  payload[4] = (ws.sensor[0].sensor_id >> 8) & 0xFF;
  payload[5] = (ws.sensor[0].sensor_id) & 0xFF;
  payload[6] = ws.sensor[0].s_type << 4;
  payload[6] |= (ws.sensor[0].startup ? 0 : 8) | ws.sensor[0].chan;
  if (ws.sensor[0].battery_ok) {
    payload[7] = 0x30;
  } else {
    payload[7] = 0x00;
  }

  if (ws.sensor[0].leak.alarm)
  {
    payload[7] |= 8;
  }
  else
  {
    payload[7] |= 4;
  }

  uint16_t crc = crc16(&payload[2], 5, 0x1021, 0x0000);
  log_d("CRC: 0x%04X", crc);

  payload[0] = crc >> 8;
  payload[1] = crc & 0xFF;

  memcpy(msg, payload, 10);

  // Return message size
  return 10;
}

void loop()
{
  String input_str;
  static String json_str;
  static Encoders encoder = Encoders::ENC_BRESSER_6IN1;
  static unsigned tx_interval = TX_INTERVAL;

  if (Serial.available())
  {
    input_str = Serial.readStringUntil('\n');
  }

  if (input_str.startsWith("{"))
  {
    json_str = input_str;
    log_i("JSON String: %s", json_str.c_str());
  }
  else if (input_str.startsWith("enc"))
  {
    if (int pos = input_str.indexOf('='))
    {
      input_str.toLowerCase();
      if (input_str.substring(pos + 1).startsWith("bresser-5in1"))
      {
        encoder = Encoders::ENC_BRESSER_5IN1;
        log_i("Encoder: Bresser 5-in-1");
      }
      else if (input_str.substring(pos + 1).startsWith("bresser-6in1"))
      {
        encoder = Encoders::ENC_BRESSER_6IN1;
        log_i("Encoder: Bresser 6-in-1");
      }
      else if (input_str.substring(pos + 1).startsWith("bresser-7in1"))
      {
        encoder = Encoders::ENC_BRESSER_7IN1;
        log_i("Encoder: Bresser 7-in-1");
      }
      else if (input_str.substring(pos + 1).startsWith("bresser-lightning"))
      {
        encoder = Encoders::ENC_BRESSER_LIGHTNING;
        log_i("Encoder: Bresser Lightning");
      }
      else if (input_str.substring(pos + 1).startsWith("bresser-leakage"))
      {
        encoder = Encoders::ENC_BRESSER_LEAKAGE;
        log_i("Encoder: Bresser Leakage");
        log_w("This encoder can currently only send raw data!");
      }
      else
      {
        log_w("Unknown encoder!");
      }
    }
  } // "enc[oder]"
  else if (input_str.startsWith("int"))
  {
    if (int pos = input_str.indexOf('='))
    {
      int val = input_str.substring(pos + 1).toInt();
      if (val > 10)
      {
        tx_interval = val;
        log_i("tx_interval: %d s", tx_interval);
      }
    }
  } // "int[erval]"
  else if (input_str != "")
  {
    log_w("Unknown command!");
  }

  uint8_t msg_buf[40];
  uint8_t msg_size;
  bool valid = true;

  msg_size = msgBegin(msg_buf);

#if defined(DATA_RAW)
  msg_size += rawPayload(encoder, &msg_buf[msg_size]);
#elif defined(DATA_GEN)
  genData(encoder);
#elif defined(DATA_JSON_CONST)
  genJson(encoder, json_str);
#endif

#if defined(DATA_JSON_CONST) || defined(DATA_JSON_INPUT)
  if (json_str.length() > 0)
  {
    valid = deSerialize(encoder, json_str);
  }
  else
  {
    valid = false;
  }
#endif

#if !defined(DATA_RAW)
  if (valid)
  {
    switch (encoder)
    {
    case Encoders::ENC_BRESSER_5IN1:
      msg_size += encodeBresser5In1Payload(&msg_buf[msg_size]);
      break;

    case Encoders::ENC_BRESSER_6IN1:
      msg_size += encodeBresser6In1Payload(&msg_buf[msg_size]);
      break;

    case Encoders::ENC_BRESSER_7IN1:
      msg_size += encodeBresser7In1Payload(&msg_buf[msg_size]);
      break;

    case Encoders::ENC_BRESSER_LIGHTNING:
      msg_size += encodeBresserLightningPayload(&msg_buf[msg_size]);
      break;

    case Encoders::ENC_BRESSER_LEAKAGE:
      msg_size += encodeBresserLeakagePayload(&msg_buf[msg_size]);
      break;

    default:
      log_e("Encoder not implemented!");
      msg_size = 0;
    }
  }
  else
  {
    msg_size = 0;
  }
#endif

  log_i("%s Transmitting packet (%d bytes)... ", TRANSCEIVER_CHIP, msg_size);
  int state = radio.transmit(msg_buf, msg_size);

  if (state == RADIOLIB_ERR_NONE)
  {
    // the packet was successfully transmitted
    log_i(" success!");

#if defined(USE_SX1276)
    // print measured data rate
    log_i("%s Datarate:\t%f bps", TRANSCEIVER_CHIP, radio.getDataRate());
#endif
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
  delay(tx_interval * 1000);
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
int add_bytes(uint8_t const message[], unsigned num_bytes)
{
  int result = 0;
  for (unsigned i = 0; i < num_bytes; ++i)
  {
    result += message[i];
  }
  return result;
}

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes, uint16_t gen, uint16_t key)
{
  uint16_t sum = 0;
  for (unsigned k = 0; k < bytes; ++k)
  {
    uint8_t data = message[k];
    for (int i = 7; i >= 0; --i)
    {
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

  for (byte = 0; byte < nBytes; ++byte)
  {
    remainder ^= message[byte] << 8;
    for (bit = 0; bit < 8; ++bit)
    {
      if (remainder & 0x8000)
      {
        remainder = (remainder << 1) ^ polynomial;
      }
      else
      {
        remainder = (remainder << 1);
      }
    }
  }
  return remainder;
}
