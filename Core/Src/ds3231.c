//
// Created by hemeng on 2025/3/29.
//
#include "ds3231.h"


#include <stdio.h>

#include "stm32h7xx_hal.h"


extern I2C_HandleTypeDef hi2c1;


// BCD转十进制
static uint8_t BCD2DEC(const uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

// 十进制转BCD
static uint8_t DEC2BCD(const uint8_t dec) {
    return dec / 10 << 4 | dec % 10;
}


static HAL_StatusTypeDef DS3231_Read(const uint8_t reg, uint8_t *buf, const uint16_t len) {
    return HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 1000);
}

static HAL_StatusTypeDef DS3231_Write(const uint8_t reg, uint8_t *buf, const uint16_t len) {
    return HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, buf, len, 1000);
}

// 设置DS3231时间
HAL_StatusTypeDef DS3231_SetTime(const DS3231_TimeType *time) {
    uint8_t buf[7] = {
        DEC2BCD(time->sec),
        DEC2BCD(time->min),
        DEC2BCD(time->hour),
        DEC2BCD(time->week),
        DEC2BCD(time->day),
        DEC2BCD(time->moon),
        DEC2BCD(time->year % 100)
    };
    return DS3231_Write(DS3231_REG_SEC, buf, sizeof(buf));
}

// 读取DS3231时间
HAL_StatusTypeDef DS3231_GetTime(DS3231_TimeType *time) {
    uint8_t buf[7];
    const HAL_StatusTypeDef ret = DS3231_Read(DS3231_REG_SEC, buf, sizeof(buf));
    if (ret != HAL_OK) return ret;
    time->sec = BCD2DEC(buf[0] & 0x7F);
    time->min = BCD2DEC(buf[1] & 0x7F);
    time->hour = BCD2DEC(buf[2] & 0x3F);
    time->week = BCD2DEC(buf[3] & 0x07);
    time->day = BCD2DEC(buf[4] & 0x3F);
    time->moon = BCD2DEC(buf[5] & 0x1F);
    time->year = 2000 + BCD2DEC(buf[6]);
    return HAL_OK;
}

HAL_StatusTypeDef DS3231_ReadSQWConfig(void) {
    DS3231_ControlReg_t ctrl_reg;
    DS3231_ReadControlReg(&ctrl_reg);
    if (ctrl_reg.bits.INTCN == 0) {
        const uint8_t rs1 = ctrl_reg.bits.RS1;
        const uint8_t rs2 = ctrl_reg.bits.RS2;
        const char *freq_str;
        if (rs2 == 0 && rs1 == 0) freq_str = "1 Hz";
        else if (rs2 == 0 && rs1 == 1) freq_str = "1.024 kHz";
        else if (rs2 == 1 && rs1 == 0) freq_str = "4.096 kHz";
        else freq_str = "8.192 kHz";
        printf("SQW 模式：方波输出，频率 = %s\r\n", freq_str);
    } else {
        printf("SQW 模式：闹钟中断输出 (INTCN=1)，A1IE=%d, A2IE=%d\r\n", ctrl_reg.bits.A1IE, ctrl_reg.bits.A2IE);
    }
    return HAL_OK;
}


/**
 * @brief 读取控制寄存器到位域结构
 * @param ctrl_reg 指向 DS3231_ControlReg_t 的指针
 * @return HAL 状态
 */
HAL_StatusTypeDef DS3231_ReadControlReg(DS3231_ControlReg_t *ctrl_reg) {
    return DS3231_Read(DS3231_REG_CONTROL, &ctrl_reg->all, sizeof(&ctrl_reg->all));
}

HAL_StatusTypeDef DS3231_WriteControlReg(DS3231_ControlReg_t *ctrl_reg) {
    return DS3231_Write(DS3231_REG_CONTROL, &ctrl_reg->all, sizeof(&ctrl_reg->all));
}

HAL_StatusTypeDef DS3231_ReadStatusReg(DS3231_StatusReg_t *status_reg) {
    return DS3231_Read(DS3231_REG_STATUS, &status_reg->all, sizeof(&status_reg->all));
}

HAL_StatusTypeDef DS3231_WriteStatusReg(DS3231_StatusReg_t *status_reg) {
    return DS3231_Write(DS3231_REG_STATUS, &status_reg->all, sizeof(&status_reg->all));
}

/**
 *
每秒触发一次

mode = 0x0F（所有字段均被屏蔽）

表示不匹配任何字段，闹钟每秒触发一次

每分钟的特定秒数触发

mode = 0x0E（仅秒字段参与匹配）

表示仅匹配秒字段，闹钟在每分钟的特定秒数触发

每天的特定时间触发

mode = 0x00（所有字段均参与匹配）

表示匹配秒、分钟、小时和日期/星期字段，闹钟在每天的特定时间触发
 */
HAL_StatusTypeDef DS3231_SetAlarm1(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t mode) {
    uint8_t buf[4];
    buf[0] = DEC2BCD(sec) | ((mode & 0x01) ? 0x80 : 0x00);
    buf[1] = DEC2BCD(min) | ((mode & 0x02) ? 0x80 : 0x00);
    buf[2] = DEC2BCD(hour) | ((mode & 0x04) ? 0x80 : 0x00);
    buf[3] = DEC2BCD(day) | ((mode & 0x08) ? 0x80 : 0x00);
    DS3231_Write(DS3231_REG_ALARM1_SEC, buf, sizeof(buf));
    DS3231_ControlReg_t ds3231_control_reg;
    DS3231_ReadControlReg(&ds3231_control_reg);
    ds3231_control_reg.bits.A1IE = 0x01;
    ds3231_control_reg.bits.INTCN = 0x01;
    DS3231_WriteControlReg(&ds3231_control_reg);
    // 清除Alarm1标志位
    DS3231_StatusReg_t ds3231_status_reg;
    DS3231_ReadStatusReg(&ds3231_status_reg);
    ds3231_status_reg.bits.A1F = 0;
    DS3231_WriteStatusReg(&ds3231_status_reg);
    return HAL_OK;
}

#ifndef NO_EXAMPLES
void ds3231_example() {
    DS3231_TimeType rtcTime;
    // 设置时间（例如：2025年03月29日星期六 14:30:00，年份仅保留后两位）
    rtcTime.sec = 30;
    rtcTime.min = 53;
    rtcTime.hour = 21;
    rtcTime.week = 6; // 星期6
    rtcTime.day = 29;
    rtcTime.moon = 3;
    rtcTime.year = 25;
    DS3231_SetTime(&rtcTime);
    DS3231_GetTime(&rtcTime);
    printf("20%02d/%02d/%02d %02d:%02d:%02d\r\n",
           rtcTime.year, rtcTime.moon, rtcTime.day,
           rtcTime.hour, rtcTime.min, rtcTime.sec);
}
#endif
