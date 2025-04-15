//
// Created by hemeng on 2025/4/6.
//
#include "crc16_modbus.h"

extern CRC_HandleTypeDef hcrc;


uint16_t CRC_Calculate(uint32_t mark, char text[], uint32_t len) {
    HAL_CRC_Calculate(&hcrc, &mark, 4);
    HAL_CRC_Accumulate(&hcrc, &len, 2);
    return HAL_CRC_Accumulate(&hcrc, (uint32_t *) text, len);
}
