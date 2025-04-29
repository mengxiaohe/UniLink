/**
 * Ciastkolog.pl (https://github.com/ciastkolog)
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 sheinz (https://github.com/sheinz)
 *
 * 本文件实现BME280传感器的数据读取、校准和补偿算法。
 */

#include "bme280.h"

#include <stdio.h>

#include "stm32h7xx_hal.h"  // 包含HAL库头文件

extern I2C_HandleTypeDef hi2c1;
#define BME280_I2C (&hi2c1)

// 定义支持64位运算（用于更高精度的压力补偿），若需要32位算法则取消相应宏定义
#define SUPPORT_64BIT 1
//#define SUPPORT_32BIT 1

// BME280的I2C地址：当SDO接地时，7位地址为0x76，8位地址为(0x76<<1)=0xEC，此处使用8位地址表示
#define BME280_ADDRESS 0xEE


uint8_t chipID;
uint8_t TrimParam[36]; // 用于存放校准数据（本例中直接读取到临时数组中）
int32_t tRaw, pRaw, hRaw;

// 校准参数，根据BME280数据手册的格式排列
uint16_t dig_T1, dig_P1;
int16_t dig_T2, dig_T3;
int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
uint8_t dig_H1;
int16_t dig_H2, dig_H4, dig_H5;
int8_t dig_H3, dig_H6;

/**
 * @brief 读取BME280存储在内部ROM中的校准参数
 * @note 从寄存器0x88开始读取25字节，再从寄存器0xE1读取7字节，共计32字节数据
 */
void TrimRead(void) {
    uint8_t trim_data[32];

    // 从寄存器0x88读取25字节校准数据
    if (HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, 0x88, I2C_MEMADD_SIZE_8BIT, trim_data, 25, HAL_MAX_DELAY) !=
        HAL_OK) {
        return;
    }
    // 从寄存器0xE1读取7字节校准数据，并存入trimdata[25..31]
    if (HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, 0xE1, I2C_MEMADD_SIZE_8BIT, trim_data + 25, 7, HAL_MAX_DELAY) !=
        HAL_OK) {
        return;
    }

    // 按照数据手册排列校准参数
    dig_T1 = (uint16_t) (trim_data[1] << 8 | trim_data[0]);
    dig_T2 = (int16_t) (trim_data[3] << 8 | trim_data[2]);
    dig_T3 = (int16_t) ((trim_data[5] << 8) | trim_data[4]);

    dig_P1 = (uint16_t) (trim_data[7] << 8 | trim_data[6]);
    dig_P2 = (int16_t) (trim_data[9] << 8 | trim_data[8]);
    dig_P3 = (int16_t) (trim_data[11] << 8 | trim_data[10]);
    dig_P4 = (int16_t) (trim_data[13] << 8 | trim_data[12]);
    dig_P5 = (int16_t) (trim_data[15] << 8 | trim_data[14]);
    dig_P6 = (int16_t) (trim_data[17] << 8 | trim_data[16]);
    dig_P7 = (int16_t) (trim_data[19] << 8 | trim_data[18]);
    dig_P8 = (int16_t) (trim_data[21] << 8 | trim_data[20]);
    dig_P9 = (int16_t) (trim_data[23] << 8 | trim_data[22]);

    dig_H1 = trim_data[24];
    dig_H2 = (int16_t) ((trim_data[26] << 8) | trim_data[25]);
    dig_H3 = (int8_t) trim_data[27];
    dig_H4 = (int16_t) ((trim_data[28] << 4) | (trim_data[29] & 0x0F));
    dig_H5 = (int16_t) ((trim_data[30] << 4) | (trim_data[29] >> 4));
    dig_H6 = (int8_t) trim_data[31];
}

/**
 * @brief 配置BME280的工作参数，包括复位、设置采样率、工作模式及滤波参数
 * @param osrs_t 温度过采样设置
 * @param osrs_p 压力过采样设置
 * @param osrs_h 湿度过采样设置
 * @param mode   工作模式（MODE_SLEEP、MODE_FORCED、MODE_NORMAL）
 * @param t_sb   待机时间设置
 * @param filter IIR滤波系数设置
 * @retval 0 配置成功，-1 配置失败
 */
int BME280_Config(const uint8_t osrs_t, const uint8_t osrs_p, const uint8_t osrs_h,
                  const uint8_t mode, const uint8_t t_sb, const uint8_t filter) {
    uint8_t data_write, datacheck;

    // 先读取校准参数
    TrimRead();

    // 复位设备：写入复位命令0xB6到复位寄存器
    data_write = 0xB6;
    if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, RESET_REG, I2C_MEMADD_SIZE_8BIT,
                          &data_write, 1, 1000) != HAL_OK) {
        return -1;
    }
    HAL_Delay(100); // 延时等待设备复位完成

    // 配置湿度过采样，写入CTRL_HUM_REG寄存器
    data_write = osrs_h;
    if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CTRL_HUM_REG, I2C_MEMADD_SIZE_8BIT,
                          &data_write, 1, 1000) != HAL_OK) {
        return -1;
    }
    HAL_Delay(100);
    HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CTRL_HUM_REG, I2C_MEMADD_SIZE_8BIT,
                     &datacheck, 1, 1000);
    if (datacheck != data_write) {
        return -1;
    }

    // 配置待机时间和IIR滤波参数，写入CONFIG_REG寄存器
    data_write = t_sb << 5 | (filter << 2);
    if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CONFIG_REG, I2C_MEMADD_SIZE_8BIT,
                          &data_write, 1, 1000) != HAL_OK) {
        return -1;
    }
    HAL_Delay(100);
    HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CONFIG_REG, I2C_MEMADD_SIZE_8BIT,
                     &datacheck, 1, 1000);
    if (datacheck != data_write) {
        return -1;
    }

    // 配置温度和压力的过采样及工作模式，写入CTRL_MEAS_REG寄存器
    data_write = osrs_t << 5 | (osrs_p << 2) | mode;
    if (HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, I2C_MEMADD_SIZE_8BIT,
                          &data_write, 1, 1000) != HAL_OK) {
        return -1;
    }
    HAL_Delay(100);
    HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, I2C_MEMADD_SIZE_8BIT,
                     &datacheck, 1, 1000);
    if (datacheck != data_write) {
        return -1;
    }

    return 0;
}

/**
 * @brief 读取BME280的原始数据（温度、压力、湿度）
 * @retval 0 读取成功，-1 读取失败（例如芯片ID不匹配）
 */
int BMEReadRaw(void) {
    // 读取芯片ID
    if (HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, ID_REG, I2C_MEMADD_SIZE_8BIT,
                         &chipID, 1, 10) != HAL_OK) {
        return -1;
    }

    // 检查芯片ID是否正确（0x60为正确ID）
    if (chipID == 0x60) {
        uint8_t RawData[8];
        // 从寄存器0xF7开始连续读取8字节数据：3字节压力，3字节温度，2字节湿度
        if (HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, PRESS_MSB_REG, I2C_MEMADD_SIZE_8BIT,
                             RawData, 8, 80) != HAL_OK) {
            return -1;
        }

        // 组合原始数据，注意温度和压力数据为20位，湿度数据为16位
        pRaw = (int32_t) ((RawData[0] << 12) | (RawData[1] << 4) | (RawData[2] >> 4));
        tRaw = (int32_t) ((RawData[3] << 12) | (RawData[4] << 4) | (RawData[5] >> 4));
        hRaw = (int32_t) ((RawData[6] << 8) | RawData[7]);

        return 0;
    }
    return -1;
}

/**
 * @brief 在强制模式下唤醒BME280进行一次测量
 * @note  该模式下，每次测量前需要调用本函数唤醒设备，测量后设备自动进入休眠
 */
void BME280_WakeUP(void) {
    uint8_t reg;
    // 读取当前测量控制寄存器配置
    HAL_I2C_Mem_Read(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, I2C_MEMADD_SIZE_8BIT,
                     &reg, 1, 1000);
    // 设置模式为强制模式
    reg |= MODE_FORCED;
    HAL_I2C_Mem_Write(BME280_I2C, BME280_ADDRESS, CTRL_MEAS_REG, I2C_MEMADD_SIZE_8BIT,
                      &reg, 1, 1000);
    HAL_Delay(100);
}

/********************* 以下为补偿计算函数（参照BME280数据手册实现） *************************/

int32_t t_fine; // 全局变量，用于存储温度补偿计算中的中间变量

/**
 * @brief 温度补偿计算（输出单位：0.01°C）
 * @param adc_T 原始温度数据
 * @retval 补偿后的温度值，实际温度需除以100
 */
int32_t BME280_compensate_T_int32(const int32_t adc_T) {
    const int32_t var1 = ((adc_T >> 3) - ((int32_t) dig_T1 << 1)) * (int32_t) dig_T2 >> 11;
    const int32_t var2 = (((adc_T >> 4) - (int32_t) dig_T1) * ((adc_T >> 4) - (int32_t) dig_T1) >> 12) *
                         (int32_t) dig_T3 >> 14;
    t_fine = var1 + var2;
    const int32_t T = t_fine * 5 + 128 >> 8;
    return T;
}

#if SUPPORT_64BIT
/**
 * @brief 压力补偿计算（64位算法，输出单位：Pa，以Q24.8格式表示）
 * @param adc_P 原始压力数据
 * @retval 补偿后的压力值，除以256即为实际压力（Pa）
 */
uint32_t BME280_compensate_P_int64(const int32_t adc_P) {
    int64_t var1 = ((int64_t) t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t) dig_P6;
    var2 = var2 + ((var1 * (int64_t) dig_P5) << 17);
    var2 = var2 + (((int64_t) dig_P4) << 35);
    var1 = (((var1 * var1 * (int64_t) dig_P3) >> 8) + ((var1 * (int64_t) dig_P2) << 12));
    var1 = ((((int64_t) 1 << 47) + var1)) * ((int64_t) dig_P1) >> 33;
    if (var1 == 0) {
        return 0; // 避免除零异常
    }
    int64_t p = 1048576 - adc_P;
    p = ((p << 31) - var2) * 3125 / var1;
    var1 = (int64_t) dig_P9 * (p >> 13) * (p >> 13) >> 25;
    var2 = (int64_t) dig_P8 * p >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t) dig_P7 << 4);
    return p;
}
#elif defined(SUPPORT_32BIT)
/**
 * @brief 压力补偿计算（32位算法，输出单位：Pa）
 * @param adc_P 原始压力数据
 * @retval 补偿后的压力值（Pa）
 */
uint32_t BME280_compensate_P_int32(int32_t adc_P) {
    int32_t var1, var2;
    uint32_t p;
    var1 = (((int32_t)t_fine) >> 1) - 64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dig_P6);
    var2 += ((var1 * ((int32_t)dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)dig_P4) << 16);
    var1 = ((((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)dig_P2) * var1) >> 1)) >> 18);
    var1 = (((32768 + var1)) * ((int32_t)dig_P1)) >> 15;
    if (var1 == 0) {
        return 0;  // 避免除零异常
    }
    p = (((uint32_t)(1048576 - adc_P)) - (var2 >> 12)) * 3125;
    if (p < 0x80000000)
        p = (p << 1) / ((uint32_t)var1);
    else
        p = (p / (uint32_t)var1) * 2;
    var1 = (((int32_t)dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));
    return p;
}
#endif

/**
 * @brief 湿度补偿计算（输出格式为Q22.10，实际湿度需除以1024，单位：%RH）
 * @param adc_H 原始湿度数据
 * @retval 补偿后的湿度值
 */
uint32_t bme280_compensate_H_int32(const int32_t adc_H) {
    int32_t v_x1_u32r = t_fine - 76800;
    v_x1_u32r = ((adc_H << 14) - ((int32_t) dig_H4 << 20) -
                 (int32_t) dig_H5 * v_x1_u32r + 16384 >> 15) *
                ((((v_x1_u32r * (int32_t) dig_H6 >> 10) *
                   ((v_x1_u32r * (int32_t) dig_H3 >> 11) + 32768) >> 10) +
                  2097152) * (int32_t) dig_H2 + 8192 >> 14);
    v_x1_u32r -= ((v_x1_u32r >> 15) * (v_x1_u32r >> 15) >> 7) *
            (int32_t) dig_H1 >> 4;
    if (v_x1_u32r < 0)
        v_x1_u32r = 0;
    if (v_x1_u32r > 419430400)
        v_x1_u32r = 419430400;
    return v_x1_u32r >> 12;
}

/**
 * @brief 执行温度、压力和湿度的测量，并更新全局变量
 */
void BME280_Measure(float *Temperature, float *Humidity, float *Pressure) {
    if (BMEReadRaw() == 0) {
        // 温度测量：若返回无效数据（0x800000），则置0
        *Temperature = tRaw == 0x800000 ? 0 : (float) BME280_compensate_T_int32(tRaw) / 100.0f;
        // 压力测量
#if SUPPORT_64BIT
        *Pressure = pRaw == 0x800000 ? 0 : (float) BME280_compensate_P_int64(pRaw) / 256.0f;
#elif defined(SUPPORT_32BIT)
        *Pressure = pRaw == 0x800000 ? 0 : (float) BME280_compensate_P_int32(hRaw) / 256.0f;
#endif
        // 湿度测量：若返回无效数据（0x8000），则置0
        *Humidity = hRaw == 0x800000 ? 0 : (float) bme280_compensate_H_int32(hRaw) / 1024.0f;
    } else {
        // 读取失败时，所有测量值均置为0
        *Temperature = *Pressure = *Humidity = 0;
    }
}


#ifndef NO_EXAMPLES
void bm3280_example() {
    BME280_Config(OSRS_2, OSRS_16, OSRS_1, MODE_NORMAL, T_SB_0p5, IIR_16);
    float Temperature, Humidity, Pressure;
    BME280_Measure(&Temperature, &Humidity, &Pressure);
}
#endif
