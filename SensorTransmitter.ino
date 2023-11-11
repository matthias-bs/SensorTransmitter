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
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "SensorTransmitter.h"
#include <RadioLib.h>

// SX1276 has the following connections:
// NSS pin:   PIN_TRANSCEIVER_CS
// DIO0 pin:  PIN_TRANSCEIVER_IRQ
// RESET pin: PIN_TRANSCEIVER_RST
// DIO1 pin:  PIN_TRANSCEIVER_GPIO
SX1276 radio = new Module(PIN_TRANSCEIVER_CS, PIN_TRANSCEIVER_IRQ, PIN_TRANSCEIVER_RST, PIN_TRANSCEIVER_GPIO);


void setup() {
  Serial.begin(115200);

  // initialize SX1276
  Serial.print(F("[SX1276] Initializing ... "));
  // carrier frequency:                   868.3 MHz
  // bit rate:                            8.22 kbps
  // frequency deviation:                 57.136417 kHz
  // Rx bandwidth:                        270.0 kHz (CC1101) / 250 kHz (SX1276)
  // output power:                        10 dBm
  // preamble length:                     40 bits
  // Preamble: AA AA AA AA AA
  // Sync: 2D D4
  int state = radio.beginFSK(868.3, 8.21, 57.136417, 250, 10, 32);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
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

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/devices/bresser_5in1.c (20220212)
//
// Example input data:
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
    uint8_t preamble[] = {0xAA, 0xAA, 0xAA, 0xAA};
    uint8_t syncword[] = {0x2D, 0xD4};
    uint8_t payload[] = {0xEA, 0xEC, 0x7F, 0xEB, 0x5F, 0xEE, 0xEF, 0xFA, 0xFE, 0x76, 0xBB, 0xFA, 0xFF,
                         0x15, 0x13, 0x80, 0x14, 0xA0, 0x11, 0x10, 0x05, 0x01, 0x89, 0x44, 0x05, 0x00};

    memcpy(msg, preamble, 4);
    memcpy(&msg[4], syncword, 2);
    memcpy(&msg[6], payload, 26);

    // Return message size
    return 4+2+26;
}

void loop() {
  Serial.print(F("[SX1276] Transmitting packet ... "));

  uint8_t msg_buf[40];

  uint8_t msg_size = encodeBresser5In1Payload(msg_buf);

  int state = radio.transmit(msg_buf, msg_size);

  // you can also transmit byte array up to 256 bytes long
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    int state = radio.transmit(byteArr, 8);
  */

  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    Serial.println(F(" success!"));

    // print measured data rate
    Serial.print(F("[SX1276] Datarate:\t"));
    Serial.print(radio.getDataRate());
    Serial.println(F(" bps"));

  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 256 bytes
    Serial.println(F("too long!"));

  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    // timeout occurred while transmitting packet
    Serial.println(F("timeout!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);

  }

  // wait for TX_INTERVAL seconds before transmitting again
  delay(TX_INTERVAL * 1000);
}