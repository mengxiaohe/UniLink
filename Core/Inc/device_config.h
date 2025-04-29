//
// Created by hemeng on 25-4-19.
//

#ifndef DEVICECONFIG_H
#define DEVICECONFIG_H
#include <stdint.h>

const uint32_t DEVICECONFIG_MAGICNUM = 0xfb7affaa;

#pragma pack(push, 1)

typedef struct {
    /**
      * 魔数
      */
    uint32_t magic_num;
    /**
     * 协议版本号
     */
    uint16_t version;
    /**
     * 保留字段
     */
    uint32_t reserved;
    /**
     * 	Payload 长度
     */
    uint32_t body_len;
    /**
     * 检验位
     */
    uint16_t checksum;
} DeviceConfig_t;
#pragma pack(pop)




DeviceConfig_t readDeviceConfig();
#endif //DEVICECONFIG_H
