//
// Created by hemeng on 2025/3/29.
//

#ifndef DS3231_H
#define DS3231_H


#define DS3231_ADDRESS  0xD0  // DS3231写地址；读时HAL会自动处理
#include <stdint.h>

// DS3231时间数据结构
typedef struct {
    uint8_t seconds; // 0~59
    uint8_t minutes; // 0~59
    uint8_t hours; // 0~23
    uint8_t day; // 星期 1~7
    uint8_t date; // 日 1~31
    uint8_t month; // 月 1~12（最高位为世纪位）
    uint8_t year; // 年（00~99）
} DS3231_TimeType;

// 函数声明
void DS3231_SetTime(const DS3231_TimeType *time);

void DS3231_GetTime(DS3231_TimeType *time);

uint8_t DS3231_ReadOneByte(uint8_t reg);

void DS3231_WriteOneByte(uint8_t reg, uint8_t data);
#endif //DS3231_H
