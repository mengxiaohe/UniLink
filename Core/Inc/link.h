//
// Created by hemeng on 2025/4/13.
//

#ifndef LINK_H
#define LINK_H
#include "err.h"
#include <string.h>

#include "ip_addr.h"

#include "tcp.h"
/**
 * 终端 0x0000 - 0x7FFE
 * 平台 0x7FFF - 0xFFFF
 */
enum MessageType : uint16_t {
    PING = 0x0000,
    UPLOAD_BIG_DATA = 0x0001,
    UPLOAD_STATUS = 0x0002,
    PONG = 0x7FFF,
    TERMINAL_UNIVERSAL_ACK = 0x7FFE,
    DOWNLOAD_BIG_DATA = 0x8000,
    OTA = 0x8001,
};

static const uint32_t NSHEAD_MAGICNUM = 0xfb7a9394;
static const uint16_t NSHEAD_VERSION = 0x0000;
static const uint32_t NSHEAD_RESERVED = 0x0000;
#pragma pack(push, 1)


struct nshead_t {
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
     * 检验位
     */
    uint16_t checksum;

    /**
  * 	Payload 长度
  */
    uint32_t body_len;

    /**
     * sn
     */
    uint8_t sn[16];


    /**
     * cmd
     */
    enum MessageType cmd;

    /**
     * 时间戳
     */
    uint32_t timestamp;

    /**
     * 消息 ID
     */
    uint8_t message_id[16];
};
#pragma pack(pop)

/**
*     const uint8_t sn[16] = {
        uid[0] >> 24 & 0xFF,
        uid[0] >> 16 & 0xFF,
        uid[0] >> 8 & 0xFF,
        uid[0] & 0xFF,
        uid[1] >> 24 & 0xFF,
        uid[1] >> 16 & 0xFF,
        uid[1] >> 8 & 0xFF,
        uid[1] & 0xFF,
        uid[2] >> 24 & 0xFF,
        uid[2] >> 16 & 0xFF,
        uid[2] >> 8 & 0xFF,
        uid[2] & 0xFF, 0x00, 0x00, 0x00
    };
 */
#define NSHEAD_DEFAULT { \
.magic_num = NSHEAD_MAGICNUM, \
.version = NSHEAD_VERSION, \
.reserved = NSHEAD_RESERVED, \
.body_len = 0, \
.checksum = 0, \
.sn = {  HAL_GetUIDw0() >> 24 & 0xFF, \
        HAL_GetUIDw0() >> 16 & 0xFF, \
        HAL_GetUIDw0() >> 8 & 0xFF, \
        HAL_GetUIDw0() & 0xFF, \
        HAL_GetUIDw1() >> 24 & 0xFF, \
        HAL_GetUIDw1() >> 16 & 0xFF, \
        HAL_GetUIDw1() >> 8 & 0xFF, \
        HAL_GetUIDw1() & 0xFF, \
        HAL_GetUIDw2() >> 24 & 0xFF, \
        HAL_GetUIDw2() >> 16 & 0xFF, \
        HAL_GetUIDw2() >> 8 & 0xFF, \
        HAL_GetUIDw2() & 0xFF, 0x00, 0x00, 0x00 }  ,\
.timestamp = 0  \
}


/* 初始化 TCP 客户端 */
void tcp_client_init(ip_addr_t server_ip);

/* 错误回调 */
void tcp_client_error(void *arg, err_t err);

/* 连接成功回调 */
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

/* 接收数据回调 */
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

struct nshead_t bytes_to_head(const uint8_t *buffer);

void send(uint8_t *buf, uint32_t body_len, enum MessageType cmd);

void send_x(uint8_t *buf, uint32_t msg_len);
#endif //LINK_H
