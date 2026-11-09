#ifndef RADIOLIB_CONFIG_OVERRIDE_H
#define RADIOLIB_CONFIG_OVERRIDE_H

#undef log_i
#define log_i(...) {}
#undef log_d
#define log_d(...) {}
#undef log_w
#define log_w(...) {}
#undef log_e
#define log_e(...) {}

// Disable unused modules to reduce flash usage
#define RADIOLIB_EXCLUDE_CC1101           (1)
#define RADIOLIB_EXCLUDE_NRF24            (1)
//#define RADIOLIB_EXCLUDE_RF69             (1) // used by SX1276
//#define RADIOLIB_EXCLUDE_SX126X           (1) // used by SX1276
#define RADIOLIB_EXCLUDE_STM32WLX         (1) // dependent on RADIOLIB_EXCLUDE_SX126X
//#define RADIOLIB_EXCLUDE_SX127X        (1)  // Keep commented for SX1276
#define RADIOLIB_EXCLUDE_SI443X           (1)
#define RADIOLIB_EXCLUDE_SX128X           (1)
#define RADIOLIB_EXCLUDE_HC05             (1)
#define RADIOLIB_EXCLUDE_JDY08            (1)
#define RADIOLIB_EXCLUDE_SERIAL           (1)
#define RADIOLIB_EXCLUDE_ESP8266          (1)
#define RADIOLIB_EXCLUDE_XBEE             (1)
#define RADIOLIB_EXCLUDE_LR11X0           (1)
#define RADIOLIB_EXCLUDE_RFM2X            (1) // dependent on RADIOLIB_EXCLUDE_SI443X

// Disable unused protocols
#define RADIOLIB_EXCLUDE_AFSK             (1)
#define RADIOLIB_EXCLUDE_PAGER            (1)
#define RADIOLIB_EXCLUDE_RTTY             (1)
#define RADIOLIB_EXCLUDE_MORSE            (1)
#define RADIOLIB_EXCLUDE_SSTV             (1)
#define RADIOLIB_EXCLUDE_FSK4             (1)
#define RADIOLIB_EXCLUDE_HELLSCHREIBER    (1)
#define RADIOLIB_EXCLUDE_AX25             (1)
#define RADIOLIB_EXCLUDE_DIRECT_RECEIVE   (1)
#define RADIOLIB_EXCLUDE_BELL             (1)
#define RADIOLIB_EXCLUDE_APRS             (1)
#define RADIOLIB_EXCLUDE_LORAWAN          (1)

// Disable LoRa modulation (keep only FSK)
#define RADIOLIB_EXCLUDE_LORA             (1)

// Disable floating point support
#define RADIOLIB_GODMODE                  (1)

// Disable static arrays to save RAM
#define RADIOLIB_STATIC_ARRAY_SIZE        (0)

#endif