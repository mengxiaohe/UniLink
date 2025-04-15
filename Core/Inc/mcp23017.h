//
// Created by hemeng on 2025/4/12.
//

#ifndef MCP23017_H
#define MCP23017_H
#include "stm32h7xx_hal.h"  // 根据MCU系列修改
/**
*
地址（Hex）	名称	读写权限	功能描述	默认值
0x00	IODIRA	R/W	Port A 输入输出方向控制	0xFF
0x01	IODIRB	R/W	Port B 输入输出方向控制	0xFF
0x02	IPOLA	R/W	Port A 输入极性反转	0x00
0x03	IPOLB	R/W	Port B 输入极性反转	0x00
0x04	GPINTENA	R/W	Port A 中断使能	0x00
0x05	GPINTENB	R/W	Port B 中断使能	0x00
0x06	DEFVALA	R/W	Port A 中断比较默认值	0x00
0x07	DEFVALB	R/W	Port B 中断比较默认值	0x00
0x08	INTCONA	R/W	Port A 中断触发模式控制	0x00
0x09	INTCONB	R/W	Port B 中断触发模式控制	0x00
0x0A	IOCON	R/W	全局配置（Port A/B 共用）	0x00
0x0B	IOCON	R/W	同上（Bank 0 模式下与 0x0A 相同）	0x00
0x0C	GPPUA	R/W	Port A 上拉电阻使能	0x00
0x0D	GPPUB	R/W	Port B 上拉电阻使能	0x00
0x0E	INTFA	R	Port A 中断标志寄存器	0x00
0x0F	INTFB	R	Port B 中断标志寄存器	0x00
0x10	INTCAPA	R	Port A 中断捕获值（锁存）	0x00
0x11	INTCAPB	R	Port B 中断捕获值（锁存）	0x00
0x12	GPIOA	R/W	Port A 输入/输出数据	0x00
0x13	GPIOB	R/W	Port B 输入/输出数据	0x00
0x14	OLATA	R/W	Port A 输出锁存器	0x00
0x15	OLATB	R/W	Port B 输出锁存器	0x00

 */
#define MCP23017_IODIRA   0x00
#define MCP23017_IODIRB   0x01
#define MCP23017_IPOLA    0x02
#define MCP23017_IPOLB    0x03
#define MCP23017_GPINTENA    0x04
#define MCP23017_GPINTENB    0x05
#define MCP23017_DEFVALA    0x06
#define MCP23017_DEFVALB    0x07
#define MCP23017_GPIOA    0x12
#define MCP23017_GPIOB    0x13
#define MCP23017_OLATA    0x14
#define MCP23017_OLATB    0x15

#define MCP23017_ADDRESS  (0x27 << 1)
typedef union {
    uint8_t all; // 整个寄存器的字节访问
    struct {
        uint8_t io0: 1;
        uint8_t io1: 1;
        uint8_t io2: 1;
        uint8_t io3: 1;
        uint8_t io4: 1;
        uint8_t io5: 1;
        uint8_t io6: 1;
        uint8_t io7: 1;
    } bits;
} MCP23017_GPIO_t;

#endif //MCP23017_H
