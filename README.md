# SensorTransmitter

Sensor Data FSK Transmitter based on SX1276 using RadioLib

This project is closely related to [BresserWeatherSensorReceiver](https://github.com/matthias-bs/BresserWeatherSensorReceiver)

## Use Cases

* Emulation of sensors for testing purposes
* Emulation of sensors while replacing the data with entirely different sensor values,
  e.g. using the manufacturer's temperature sensor protocol to display snow depth instead. 

## Sensor Data Provisioning Options

1. Raw Data
   ```
   uint8_t payload[] = {0xEA, 0xEC, 0x7F, 0xEB, 0x5F, 0xEE, 0xEF, 0xFA, 0xFE, 0x76, 0xBB, 0xFA, 0xFF,
                         0x15, 0x13, 0x80, 0x14, 0xA0, 0x11, 0x10, 0x05, 0x01, 0x89, 0x44, 0x05, 0x00};
   ```
2. [class WeatherSensor](https://github.com/matthias-bs/BresserWeatherSensorReceiver/blob/main/src/WeatherSensor.h)
3. JSON Data
   
   Readable format:
   ```
   {"sensor_id": 255, "s_type": 1, "chan": 0, "startup": 0, "battery_ok": 1, "temp_c": 12.3, "humidity": 44, "wind_gust_meter_sec": 3.3, "wind_avg_meter_sec": 2.2, "wind_direction_deg": 111.1, "rain_mm": 123.4}
   ```
   
   Source code:
   ```
   char json[] =
      "{\"sensor_id\":255,\"s_type\":1,\"chan\":0,\"startup\":0,\"battery_ok\":1,\"temp_c\":12.3,\
        \"humidity\":44,\"wind_gust_meter_sec\":3.3,\"wind_avg_meter_sec\":2.2,\"wind_direction_deg\":111.1,\
        \"rain_mm\":123.4}";
   ```

## Supported Protocols

- [x] Bresser 5-in-1
- [ ] Bresser 6-in-1
- [ ] Bresser 7-in-1
- [ ] Bresser Lightning
- [ ] Bresser Leakage
