
#ifndef MICROPY_INCLUDED_ESP8266_ESPAPA102_H
#define MICROPY_INCLUDED_ESP8266_ESPAPA102_H

void esp_apa102_write(uint8_t clockPin, uint8_t dataPin, uint8_t *pixels, uint32_t numBytes);

#endif 
