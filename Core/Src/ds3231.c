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
    return (dec / 10) << 4 | dec % 10;
}

// 写一个字节到DS3231指定寄存器
void DS3231_WriteOneByte(const uint8_t reg, uint8_t data) {
    if (HAL_OK != HAL_I2C_Mem_Write(&hi2c1, DS3231_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 1000)) {
        printf("Failed to write to DS3231\n");
    }
}

// 从DS3231指定寄存器读一个字节
uint8_t DS3231_ReadOneByte(const uint8_t reg) {
    uint8_t data;
    if (HAL_OK != HAL_I2C_Mem_Read(&hi2c1, DS3231_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 1000)) {
        printf("Failed to read from DS3231\n");
    }
    return data;
}

// 设置DS3231时间
void DS3231_SetTime(const DS3231_TimeType *time) {
    DS3231_WriteOneByte(0x00, DEC2BCD(time->seconds)); // 秒
    DS3231_WriteOneByte(0x01, DEC2BCD(time->minutes)); // 分
    DS3231_WriteOneByte(0x02, DEC2BCD(time->hours)); // 时（24小时制）
    DS3231_WriteOneByte(0x03, DEC2BCD(time->day)); // 星期
    DS3231_WriteOneByte(0x04, DEC2BCD(time->date)); // 日
    DS3231_WriteOneByte(0x05, DEC2BCD(time->month)); // 月（最高位表示世纪，可忽略）
    DS3231_WriteOneByte(0x06, DEC2BCD(time->year)); // 年（00~99）
}

// 读取DS3231时间
void DS3231_GetTime(DS3231_TimeType *time) {
    time->seconds = BCD2DEC(DS3231_ReadOneByte(0x00));
    time->minutes = BCD2DEC(DS3231_ReadOneByte(0x01));
    time->hours = BCD2DEC(DS3231_ReadOneByte(0x02));
    time->day = BCD2DEC(DS3231_ReadOneByte(0x03));
    time->date = BCD2DEC(DS3231_ReadOneByte(0x04));
    time->month = BCD2DEC(DS3231_ReadOneByte(0x05));
    time->year = BCD2DEC(DS3231_ReadOneByte(0x06));
}


#ifndef NO_EXAMPLES
void ds3231_example() {
    DS3231_TimeType rtcTime;
    // 设置时间（例如：2025年03月29日星期六 14:30:00，年份仅保留后两位）
    rtcTime.seconds = 30;
    rtcTime.minutes = 53;
    rtcTime.hours = 21;
    rtcTime.day = 6; // 星期6
    rtcTime.date = 29;
    rtcTime.month = 3;
    rtcTime.year = 25;
    DS3231_SetTime(&rtcTime);
    DS3231_GetTime(&rtcTime);
    printf("20%02d/%02d/%02d %02d:%02d:%02d\r\n",
           rtcTime.year, rtcTime.month, rtcTime.date,
           rtcTime.hours, rtcTime.minutes, rtcTime.seconds);
}
#endif
