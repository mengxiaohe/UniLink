//
// Created by hemeng on 2025/4/12.
//
#include "SHTC3.h"

#include <stdio.h>


typedef enum {
    READ_ID = 0xEFC8, // 命令：读取ID寄存器
    SOFT_RESET = 0x805D, // 软复位
    SLEEP = 0xB098, // 进入睡眠模式
    WAKEUP = 0x3517, // 唤醒设备
    MEAS_T_RH_POLLING = 0x7866, // 测量：先读取温度，轮询模式（禁用时钟拉伸）
    MEAS_T_RH_CLOCKSTR = 0x7CA2, // 测量：先读取温度，时钟拉伸模式
    MEAS_RH_T_POLLING = 0x58E0, // 测量：先读取湿度，轮询模式（禁用时钟拉伸）
    MEAS_RH_T_CLOCKSTR = 0x5C24 // 测量：先读取湿度，时钟拉伸模式
} etCommands;

extern I2C_HandleTypeDef hi2c3;


SHTC3_MeasureData shtc3Read;
SHTC3_Id shtc3_id;


static uint8_t SHTC3_CheckCrc(const uint8_t *data, uint8_t length, uint8_t checksum) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (crc << 1) ^ CRC_POLYNOMIAL : (crc << 1);
        }
    }
    return (crc == checksum) ? CRC_CHECK_PASS : CRC_CHECK_FAIL;
}

static HAL_StatusTypeDef SHTC3_SendCommand(uint16_t cmd) {
    uint8_t cmdBuffer[2] = {(uint8_t) (cmd >> 8), (uint8_t) (cmd)};
    return HAL_I2C_Master_Transmit(&hi2c3, SHTC3_Aaddress_W, cmdBuffer, sizeof(cmdBuffer), 1000);
}

/**
 * @brief 读取 SHTC3 传感器的 ID
 * @param id 指向用于存储传感器 ID 的变量的指针
 * @retval HAL 状态码
 */
HAL_StatusTypeDef SHTC3_GetId(uint16_t *id) {
    HAL_StatusTypeDef error = SHTC3_SendCommand(READ_ID);
    if (HAL_OK == error) {
        error = HAL_I2C_Master_Receive(&hi2c3, SHTC3_Aaddress_R, (uint8_t *) &shtc3_id, sizeof(shtc3_id), 1000);
        if (HAL_OK == error) {
            const uint8_t bytes[2] = {
                shtc3_id.IdMSB, shtc3_id.IdLSB
            };
            if (!SHTC3_CheckCrc(bytes, 2, shtc3_id.idCRC)) {
                return HAL_ERROR;
            }
            *id = (bytes[0] << 8) | bytes[1];
            return HAL_OK;
        }
        return error;
    }
    return error;
}

//------------------------------------------------------------------------------
static float SHTC3_CalcTemperature(uint16_t rawValue) {
    // calculate temperature [°C]
    // T = -45 + 175 * rawValue / 2^16
    return 175 * (float) rawValue / 65536.0f - 45.0f;
}

//------------------------------------------------------------------------------
static float SHTC3_CalcHumidity(uint16_t rawValue) {
    // calculate relative humidity [%RH]
    // RH = rawValue / 2^16 * 100
    return 100 * (float) rawValue / 65536.0f;
}


HAL_StatusTypeDef SHTC3_GetTempAndHumi(float *temp, float *humi) {
    HAL_StatusTypeDef error = SHTC3_SendCommand(MEAS_RH_T_CLOCKSTR);
    if (HAL_OK == error) {
        HAL_Delay(15);
        error = HAL_I2C_Master_Receive(&hi2c3, SHTC3_Aaddress_R, (uint8_t *) &shtc3Read, sizeof(shtc3Read), 1000);
        if (HAL_OK == error) {
            uint8_t bytes[2] = {
                shtc3Read.temperatureMSB, shtc3Read.temperatureLSB
            };
            if (SHTC3_CheckCrc(bytes, 2, shtc3Read.temperatureCRC)) {
                bytes[0] = shtc3Read.HumidityMSB;
                bytes[1] = shtc3Read.HumidityLSB;
                if (SHTC3_CheckCrc(bytes, 2, shtc3Read.HumidityCRC)) {
                    *humi = SHTC3_CalcHumidity(shtc3Read.HumidityMSB << 8 | shtc3Read.HumidityLSB);
                    *temp = SHTC3_CalcTemperature(shtc3Read.temperatureMSB << 8 | shtc3Read.temperatureLSB);
                    return error;
                }
            }
            return HAL_ERROR;
        }
        return error;
    }
    return error;
}

HAL_StatusTypeDef SHTC3_Wakeup() {
    const HAL_StatusTypeDef error = SHTC3_SendCommand(WAKEUP);
    if (HAL_OK == error) {
        HAL_Delay(10);
    }
    return error;
}

HAL_StatusTypeDef SHTC3_Sleep() {
    const HAL_StatusTypeDef error = SHTC3_SendCommand(SLEEP);
    if (HAL_OK == error) {
        HAL_Delay(10);
    }
    return error;
}

HAL_StatusTypeDef SHTC3_SoftReset() {
    const HAL_StatusTypeDef error = SHTC3_SendCommand(SOFT_RESET);
    if (HAL_OK == error) {
        HAL_Delay(10);
    }
    return error;
}
