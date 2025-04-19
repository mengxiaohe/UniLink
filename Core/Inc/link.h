//
// Created by hemeng on 2025/4/13.
//

#ifndef LINK_H
#define LINK_H
#include "err.h"
#include <string.h>

#include "ip_addr.h"

#include "tcp.h"


static const uint32_t NSHEAD_MAGICNUM = 0xfb7a9394;
#pragma pack(1)
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
     * 	Payload 长度
     */
    uint32_t body_len;
    /**
     * 检验位
     */
    uint16_t checksum;
};


#define NSHEAD_DEFAULT { \
.magic_num = NSHEAD_MAGICNUM, \
.version = 0, \
.reserved = 0, \
.body_len = 0, \
.checksum = 0 \
}


/* 初始化 TCP 客户端 */
void tcp_client_init(ip_addr_t server_ip);

/* 错误回调 */
void tcp_client_error(void *arg, err_t err);

/* 连接成功回调 */
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

/* 接收数据回调 */
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

struct nshead_t bytes_to_struct(const uint8_t *buffer);
void send(char *buf);
#endif //LINK_H
