//
// Created by hemeng on 2025/4/6.
//
#include "crc16_modbus.h"



#define POLYNOMIAL 0xA001

uint16_t crc16_table[256];

// 初始化CRC16查找表
void init_crc16_table(void) {
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ POLYNOMIAL;
            else
                crc >>= 1;
        }
        crc16_table[i] = crc;
    }
}

// 计算CRC16（Modbus）值
uint16_t crc16_modbus(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    while (length--) {
        const uint8_t table_index = (crc ^ *data++) & 0xFF;
        crc = (crc >> 8) ^ crc16_table[table_index];
    }
    return crc;
}

uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
