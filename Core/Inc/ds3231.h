//
// Created by hemeng on 2025/3/29.
//

#ifndef DS3231_H
#define DS3231_H


#define DS3231_ADDRESS  (0x68 << 1)  // DS3231写地址；读时HAL会自动处理


/* 寄存器地址 */
#define DS3231_REG_SEC            0x00  // 秒寄存器
#define DS3231_REG_MIN            0x01  // 分寄存器
#define DS3231_REG_HOUR           0x02  // 时寄存器
#define DS3231_REG_DAY            0x03  // 星期寄存器
#define DS3231_REG_DATE           0x04  // 日期寄存器
#define DS3231_REG_MONTH          0x05  // 月寄存器
#define DS3231_REG_YEAR           0x06  // 年寄存器
#define DS3231_REG_ALARM1_SEC     0x07  // 闹钟1 秒
#define DS3231_REG_ALARM1_MIN     0x08  // 闹钟1 分
#define DS3231_REG_ALARM1_HOUR    0x09  // 闹钟1 时
#define DS3231_REG_ALARM1_DAYDATE 0x0A  // 闹钟1 日/周
#define DS3231_REG_ALARM2_MIN     0x0B  // 闹钟2 分
#define DS3231_REG_ALARM2_HOUR    0x0C  // 闹钟2 时
#define DS3231_REG_ALARM2_DAYDATE 0x0D  // 闹钟2 日/周
#define DS3231_REG_CONTROL        0x0E  // 控制寄存器
#define DS3231_REG_STATUS         0x0F  // 状态寄存器

#include <stdint.h>
#include <sys/time.h>

#include "stm32h7xx_hal.h"

// DS3231时间数据结构
typedef struct {
    uint8_t sec, min, hour, week, day, month; //秒 0~59 //分 0~59 //时 0~23 // 星期 1~7 // 日 1~31 // 月 1~12（最高位为世纪位）
    uint16_t year; // 年（00~99）
} DS3231_TimeType;

/**
 * @brief DS3231 控制寄存器（0x0E）位域映射
 * intcn =1 为 闹钟
 * intcn =0 为 方波模式
 * rs1,rs2=0 1 Hz
 * rs1=1,rs2=0 1.024 kHz
 * rs1=0,rs2=1 4.096 kHz
 * rs1=1,rs2=1 8.192 kHz
 */
typedef union {
    uint8_t all; /**< 整个寄存器字节访问 */
    struct {
        uint8_t A1IE: 1; /**< Bit0: 闹钟1 中断使能 (Alarm1 Interrupt Enable) */
        uint8_t A2IE: 1; /**< Bit1: 闹钟2 中断使能 (Alarm2 Interrupt Enable) */
        uint8_t INTCN: 1; /**< Bit2: 中断控制 (0=SQW 方波输出, 1=闹钟中断输出) */
        uint8_t RS1: 1; /**< Bit3: 方波频率选择位1 (Rate Select 1) */
        uint8_t RS2: 1; /**< Bit4: 方波频率选择位2 (Rate Select 2) */
        uint8_t CONV: 1; /**< Bit5: 温度转换启动 (Convert Temperature, 一次性转换) 执行温度补偿算法 */
        uint8_t BBSQW: 1; /**< Bit6: 备用电池方波输出使能 (Battery‑Backed Square‑Wave Enable) */
        uint8_t EOSC: 1; /**< Bit7: 电池供电时,振荡器使能 (0=运行, 1=停止 Oscillator Stop)  */
    } bits;
} DS3231_ControlReg_t;

typedef union {
    uint8_t all; // 整个寄存器的字节访问
    struct {
        uint8_t A1F: 1; // Bit 0: Alarm 1 标志位
        uint8_t A2F: 1; // Bit 1: Alarm 2 标志位
        uint8_t BSY: 1; // Bit 2: 温度转换忙标志
        uint8_t EN32kHz: 1; // Bit 3: 32kHz 输出使能
        uint8_t RSVD: 2; // Bit 4-5: 保留位
        uint8_t CRATE1: 1; // Bit 6: 温度转换速率控制位 1
        uint8_t CRATE0: 1; // Bit 7: 温度转换速率控制位 0
    } bits;
} DS3231_StatusReg_t;

// 函数声明
HAL_StatusTypeDef DS3231_SetTime(const DS3231_TimeType *time);

HAL_StatusTypeDef DS3231_GetTime(DS3231_TimeType *time);

time_t DS3231_GetTimestamp();

HAL_StatusTypeDef DS3231_ReadSQWConfig(void);


HAL_StatusTypeDef DS3231_WriteControlReg(DS3231_ControlReg_t *ctrl_reg);

HAL_StatusTypeDef DS3231_ReadControlReg(DS3231_ControlReg_t *ctrl_reg);

HAL_StatusTypeDef DS3231_ReadStatusReg(DS3231_StatusReg_t *status_reg);

HAL_StatusTypeDef DS3231_WriteStatusReg(DS3231_StatusReg_t *status_reg);

HAL_StatusTypeDef DS3231_SetAlarm1(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t mode);
#endif //DS3231_H
