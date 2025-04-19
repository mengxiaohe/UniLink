//
// Created by hemeng on 2025/4/12.
//

#ifndef SHTC3_H
#define SHTC3_H
#include "stm32h7xx_hal.h"
#define CRC_POLYNOMIAL  0x131 // P(x) = x^8 + x^5 + x^4 + 1 = 100110001
#define SHTC3_Aaddress_W 0xE0
#define SHTC3_Aaddress_R 0xE1
#define CRC_CHECK_PASS 1
#define CRC_CHECK_FAIL 0
typedef struct {
    uint8_t HumidityMSB;
    uint8_t HumidityLSB;
    uint8_t HumidityCRC;

    uint8_t temperatureMSB;
    uint8_t temperatureLSB;
    uint8_t temperatureCRC;
} SHTC3_MeasureData;

typedef struct {
    uint8_t IdMSB;
    uint8_t IdLSB;
    uint8_t idCRC;
} SHTC3_Id;

void SHTC3_Init();

HAL_StatusTypeDef SHTC3_GetId(uint16_t *id);

HAL_StatusTypeDef SHTC3_GetTempAndHumi(uint16_t *temp, uint16_t *humi);

HAL_StatusTypeDef SHTC3_Wakeup(void);
HAL_StatusTypeDef SHTC3_Sleep(void);
HAL_StatusTypeDef SHTC3_SoftReset(void);
#endif //SHTC3_H
