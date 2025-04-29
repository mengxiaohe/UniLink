/**
 * Ciastkolog.pl (https://github.com/ciastkolog)
 * 
 * The MIT License (MIT)
 * Copyright (c) 2016 sheinz (https://github.com/sheinz)
 *
 * 本头文件定义了BME280传感器的接口函数和相关参数
 */

#ifndef __BMP280_H__
#define __BMP280_H__

#include <stdint.h>


/* BME280配置参数说明：
 *
 * @osrs：温度/压力/湿度的过采样设置，用于提高测量精度
 *        当设置为OSRS_OFF时，表示跳过对应测量
 *        可选值有 OSRS_1, OSRS_2, OSRS_4, OSRS_8, OSRS_16 等
 *
 * @mode：设备工作模式
 *         MODE_SLEEP：休眠模式
 *         MODE_FORCED：强制模式（单次测量后自动进入休眠，需要每次调用BME280_WakeUP唤醒）
 *         MODE_NORMAL：连续测量模式（参见数据手册第16页）
 *
 * @t_sb：待机时间，用于控制正常模式下测量的间隔时间（详见数据手册第16和30页）
 *
 * @filter：IIR滤波系数，用于减少短期数据波动（详见数据手册第18和30页）
 */

/**
 * @brief 配置BME280的工作参数
 * @param osrs_t 温度过采样设置
 * @param osrs_p 压力过采样设置
 * @param osrs_h 湿度过采样设置
 * @param mode   工作模式（休眠、强制或正常）
 * @param t_sb   待机时间设置
 * @param filter IIR滤波系数设置
 * @return 配置成功返回0，失败返回-1
 */
int BME280_Config(uint8_t osrs_t, uint8_t osrs_p, uint8_t osrs_h, uint8_t mode, uint8_t t_sb, uint8_t filter);

/**
 * @brief 读取传感器内部存储的校准参数
 */
void TrimRead(void);

/**
 * @brief 在强制模式下唤醒BME280进行一次测量
 * @note 每次测量前需要调用此函数唤醒设备
 */
void BME280_WakeUP(void);

/**
 * @brief 执行温度、压力和湿度测量
 */
void BME280_Measure(float *Temperature, float *Humidity, float *Pressure) ;

/* 以下为BME280使用的宏定义 */

// 过采样设置
#define OSRS_OFF        0x00    // 关闭测量
#define OSRS_1          0x01    // 1倍过采样
#define OSRS_2          0x02    // 2倍过采样
#define OSRS_4          0x03    // 4倍过采样
#define OSRS_8          0x04    // 8倍过采样
#define OSRS_16         0x05    // 16倍过采样

// 工作模式
#define MODE_SLEEP      0x00    // 休眠模式
#define MODE_FORCED     0x01    // 强制模式
#define MODE_NORMAL     0x03    // 正常模式

// 待机时间（单位见数据手册）
#define T_SB_0p5        0x00
#define T_SB_62p5       0x01
#define T_SB_125        0x02
#define T_SB_250        0x03
#define T_SB_500        0x04
#define T_SB_1000       0x05
#define T_SB_10         0x06
#define T_SB_20         0x07

// IIR滤波系数
#define IIR_OFF         0x00    // 关闭滤波
#define IIR_2           0x01    // 系数为2
#define IIR_4           0x02    // 系数为4
#define IIR_8           0x03    // 系数为8
#define IIR_16          0x04    // 系数为16

// 寄存器地址定义
#define ID_REG          0xD0    // 芯片ID寄存器
#define RESET_REG       0xE0    // 复位寄存器
#define CTRL_HUM_REG    0xF2    // 湿度控制寄存器
#define STATUS_REG      0xF3    // 状态寄存器
#define CTRL_MEAS_REG   0xF4    // 温度和压力控制寄存器
#define CONFIG_REG      0xF5    // 配置寄存器
#define PRESS_MSB_REG   0xF7    // 压力数据起始寄存器

#endif  // __BMP280_H__
