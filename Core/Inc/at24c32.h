//
// Created by hemeng on 2025/3/29.
//

#ifndef AT24C32_H
#define AT24C32_H
#define EEPROM_I2C_ADDR         (0x57 << 1)    // 7-bit地址左移1位后的值 (0x57 << 1)
#define EEPROM_PAGE_SIZE       32      // 页大小（AT24C32为32字节）
#define EEPROM_WRITE_DELAY     10      // 写入延迟（单位：ms，根据手册设置）
#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
// 函数声明
HAL_StatusTypeDef EEPROM_Init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef EEPROM_Write(uint16_t memAddr, const uint8_t *data, uint16_t size);

HAL_StatusTypeDef EEPROM_Read(uint16_t memAddr, uint8_t *data, uint16_t size);


#endif //AT24C32_H
