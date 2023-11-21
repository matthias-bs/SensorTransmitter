[![CI](https://github.com/matthias-bs/SensorTransmitter/actions/workflows/CI.yml/badge.svg)](https://github.com/matthias-bs/SensorTransmitter/actions/workflows/CI.yml)
[![GitHub release](https://img.shields.io/github/release/matthias-bs/SensorTransmitter?maxAge=3600)](https://github.com/matthias-bs/SensorTransmitter/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](https://github.com/matthias-bs/SensorTransmitter/blob/main/LICENSE)

# SensorTransmitter

Sensor Data FSK Transmitter based on RadioLib using SX1276 or CC1101 

This project is closely related to [BresserWeatherSensorReceiver](https://github.com/matthias-bs/BresserWeatherSensorReceiver)

## Use Cases

* Emulation of sensors for testing purposes
* Emulation of sensors while replacing the data with entirely different sensor values,
  e.g. using the manufacturer's temperature sensor protocol to display snow depth instead.
* Repeater (combined with receiver)

## Supported Protocols

- [x] Bresser 5-in-1
- [x] Bresser 6-in-1
- [x] Bresser 7-in-1
- [x] Bresser Lightning
- [ ] Bresser Leakage

## Sensor Data Provisioning Options

See sensor types in [WeatherSensor.h](https://github.com/matthias-bs/BresserWeatherSensorReceiver/blob/main/src/WeatherSensor.h)

Select option in [SensorTransmitter.h](SensorTransmitter.h).

### Raw Data
   ```
   uint8_t payload[] = {0xEA, 0xEC, 0x7F, 0xEB, 0x5F, 0xEE, 0xEF, 0xFA, 0xFE, 0x76, 0xBB, 0xFA, 0xFF,
                         0x15, 0x13, 0x80, 0x14, 0xA0, 0x11, 0x10, 0x05, 0x01, 0x89, 0x44, 0x05, 0x00};
   ```
### [class WeatherSensor](https://github.com/matthias-bs/BresserWeatherSensorReceiver/blob/main/src/WeatherSensor.h)

### JSON Data as Constant String
   
   ```
   const char json[] =
      "{\"sensor_id\":255,\"s_type\":1,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"temp_c\":12.3,\
        \"humidity\":44,\"wind_gust_meter_sec\":3.3,\"wind_avg_meter_sec\":2.2,\"wind_direction_deg\":111.1,\
        \"rain_mm\":123.4}";
   ```

### JSON Data as Input from Serial Console - Examples

#### Bresser 5-in-1 Protocol - Weather Sensor

   ```
   {"sensor_id": 255, "s_type": 1, "chan": 0, "startup": 0, "battery_ok": 1, "temp_c": 12.3, "humidity": 44, "wind_gust_meter_sec": 3.3, "wind_avg_meter_sec": 2.2, "wind_direction_deg": 111.1, "rain_mm": 123.4}
   ```

#### Bresser 6-in-1 Protocol - Soil Temperature and Moisture

   ```
   {"sensor_id": 4294967295, "s_type": 4, "chan": 0, "startup": 0, "battery_ok": 1, "temp_c": 12.3, "moisture": 44}
   ```

#### Bresser 6-in-1 Protocol - Weather Sensor

   ```
   {"sensor_id": 4294967295, "s_type": 1, "chan": 0, "startup": 0, "battery_ok": 1, "temp_c": 12.3, "humidity": 44, "wind_gust_meter_sec": 3.3, "wind_avg_meter_sec": 2.2, "wind_direction_deg": 111.1, "rain_mm": 123.4, "uv": 7.8}
   ```

#### Bresser 7-in-1 Protocol - Weather Sensor

   ```
   {"sensor_id": 65535, "s_type": 1, "chan": 0, "startup": 0, "battery_ok": 1, "temp_c": 12.3, "humidity": 44, "wind_gust_meter_sec": 3.3, "wind_avg_meter_sec": 2.2, "wind_direction_deg": 111.1, "rain_mm": 123.4, "uv": 7.8, "light_klx": 123.456}
   ```

#### Bresser 7-in-1 Protocol - Particulate Matter

   ```
   {"sensor_id": 65535, "s_type": 8, "chan": 0, "startup": 0, "battery_ok": 1, "pm_2_5": 2345, "pm_10": 1234}
   ```

#### Bresser Lightning Sensor

   ```
   {"sensor_id": 65535, "s_type": 9, "chan": 0, "startup": 0, "battery_ok": 1, "strike_count": 11, "distance_km": 7}
   ```

#### Bresser Leakage Sensor

   ```
   {"sensor_id":4294967295, "s_type": 5, "chan": 0, "startup": 0, "battery_ok": 1, "alarm": 1}
   ```

## Serial Port Control

**Note:** No additional spaces are allowed in commands! (But spaces are permitted in JSON strings.)

| Command                 | Examples                                      | Description           |
| ----------------------- | --------------------------------------------- | --------------------- |
| `{...}`                 | see above                                     | Set JSON message data |  
| `enc[oder]=<encoder>`   | `enc=bresser-5in1`<br>`enc=bresser-6in1`<br>`enc=bresser-7in1`<br>`enc=bresser-lightning`<br>`enc=bresser-leakage` | Select encoder        |
| `int[erval]=<interval>` | `int=20`                                      | Set transmit interval in seconds<br>(must be > 10) |
