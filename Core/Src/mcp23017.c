//
// Created by hemeng on 2025/4/12.
//
#include "mcp23017.h"


/**
 *
MCP23017_GPIO_t gpio_a;
    gpio_a.bits.io0 = 0;
    gpio_a.bits.io1 = 0;
    gpio_a.bits.io2 = 0;
    gpio_a.bits.io3 = 0;
    gpio_a.bits.io4 = 1;
    gpio_a.bits.io5 = 1;
    gpio_a.bits.io6 = 1;
    gpio_a.bits.io7 = 1;
    MCP23017_GPIO_t gpio_b;
    gpio_b.bits.io0 = 0;
    gpio_b.bits.io1 = 0;
    gpio_b.bits.io2 = 0;
    gpio_b.bits.io3 = 0;
    gpio_b.bits.io4 = 0;
    gpio_b.bits.io5 = 0;
    gpio_b.bits.io6 = 0;
    gpio_b.bits.io7 = 0;
    HAL_I2C_Mem_Write(&hi2c3, MCP23017_ADDRESS, MCP23017_IODIRA, I2C_MEMADD_SIZE_8BIT, &gpio_a.all, 1,
                      100);
    HAL_I2C_Mem_Write(&hi2c3, MCP23017_ADDRESS, MCP23017_IODIRB, I2C_MEMADD_SIZE_8BIT, &gpio_b.all, 1,
                      100);
    HAL_I2C_Mem_Write(&hi2c3, MCP23017_ADDRESS, MCP23017_OLATA, I2C_MEMADD_SIZE_8BIT, &gpio_a.all, 1,
                      100);
    HAL_I2C_Mem_Write(&hi2c3, MCP23017_ADDRESS, MCP23017_OLATB, I2C_MEMADD_SIZE_8BIT, &gpio_b.all, 1,
                      100);
 */
