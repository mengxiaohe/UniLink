//
// Created by hemeng on 2025/4/6.
//

#ifndef CRC16_MODBUS_H
#define CRC16_MODBUS_H

#include <stdint.h>

#include "stm32h7xx_hal.h"


uint16_t CRC_Calculate(uint32_t mark, char text[], uint32_t len);
#endif //CRC16_MODBUS_H
