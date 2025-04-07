//
// Created by hemeng on 2025/4/6.
//

#ifndef CRC16_MODBUS_H
#define CRC16_MODBUS_H
#include <stddef.h>
#include <stdint.h>
void init_crc16_table();
uint16_t crc16_modbus(const uint8_t *data, size_t length);

uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t length);
#endif //CRC16_MODBUS_H
