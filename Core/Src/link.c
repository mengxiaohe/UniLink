//
// Created by hemeng on 2025/4/13.
//

#include "link.h"

#include "cJSON.h"
#include "crc.h"
#include "ds3231.h"
#include "iwdg.h"
#include "lwip.h"


/* TCP PCB 句柄 */
static struct tcp_pcb *tcp_client_pcb;
extern RNG_HandleTypeDef hrng;

/* 初始化并启动 TCP 客户端 */
void tcp_client_init(ip_addr_t server_ip) {
    /* 1. 创建新的 TCP 控制块 */
    tcp_client_pcb = tcp_new_ip6();
    if (tcp_client_pcb == NULL) {
        // 分配失败，处理错误
        return;
    }
    ip_addr_t local_ip6;
    ip_addr_set_zero_ip6(&local_ip6);
    uint32_t rnd;
    HAL_RNG_GenerateRandomNumber(&hrng, &rnd);
    const uint16_t local_port = rnd % (65535u - 2000u + 1u) + 2000u;
    tcp_bind(tcp_client_pcb, &local_ip6, local_port);
    /* 2. 注册错误回调 */
    tcp_err(tcp_client_pcb, tcp_client_error);


    /* 4. 发起连接（三次握手） */
    tcp_connect(tcp_client_pcb,
                &server_ip,
                8033,
                tcp_client_connected);
}

void Fill_MessageID_With_HWRNG(uint8_t *message_id) {
    for (int i = 0; i < 16; i += 4) {
        uint32_t random32;
        if (HAL_RNG_GenerateRandomNumber(&hrng, &random32) != HAL_OK) {
            Error_Handler();
        }
        // 将32位随机数拆分为4个字节
        message_id[i] = (random32 >> 24) & 0xFF;
        message_id[i + 1] = (random32 >> 16) & 0xFF;
        message_id[i + 2] = (random32 >> 8) & 0xFF;
        message_id[i + 3] = random32 & 0xFF;
    }
}

//htons   htonl
uint8_t *head_to_bytes(const struct nshead_t *head) {
    static uint8_t buffer[sizeof(struct nshead_t)];
    const uint32_t magic_num_net = htonl(head->magic_num);
    memcpy(buffer, &magic_num_net, 4);
    const uint16_t version_net = htons(head->version);
    memcpy(buffer + 4, &version_net, 2);
    const uint32_t reserved_net = htonl(head->reserved);
    memcpy(buffer + 6, &reserved_net, 4);
    const uint16_t checksum_net = htons(head->checksum);
    memcpy(buffer + 10, &checksum_net, 2);
    const uint32_t body_len_net = htonl(head->body_len);
    memcpy(buffer + 12, &body_len_net, 4);
    for (int i = 0; i < sizeof(head->sn); ++i) {
        buffer[16 + i] = head->sn[i];
    }
    buffer[33] = head->cmd & 0xFF;
    buffer[32] = head->cmd >> 8 & 0xFF;
    const uint32_t timestamp_net = htonl(head->timestamp);
    memcpy(buffer + 34, &timestamp_net, 4);
    for (int i = 0; i < sizeof(head->message_id); ++i) {
        buffer[38 + i] = head->message_id[i];
    }
    return buffer;
}

struct nshead_t bytes_to_head(const uint8_t *buffer) {
    struct nshead_t head;
    uint32_t magic_num_net;
    memcpy(&magic_num_net, buffer, 4);
    head.magic_num = ntohl(magic_num_net);
    head.version = (uint16_t) buffer[4] << 8
                   | buffer[5];
    head.reserved = (uint32_t) buffer[6] << 24
                    | buffer[7] << 16
                    | buffer[8] << 8
                    | buffer[9];
    head.checksum = (uint16_t) buffer[10] << 8
                    | buffer[11];
    head.body_len = (uint32_t) buffer[12] << 24
                    | buffer[13] << 16
                    | buffer[14] << 8
                    | buffer[15];
    for (int i = 0; i < 16; ++i) {
        head.sn[i] = buffer[16 + i];
    }
    head.cmd = (enum MessageType) (uint16_t) buffer[32] << 8
               | buffer[33];
    head.timestamp = (uint32_t) buffer[34] << 24
                     | buffer[35] << 16
                     | buffer[36] << 8
                     | buffer[37];
    for (int i = 0; i < 16; ++i) {
        head.message_id[i] = buffer[38 + i];
    }
    return head;
}

/* 连接建立成功后的回调 */
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        return err;
    }
    /* 注册接收数据回调 */
    tcp_recv(tpcb, tcp_client_recv);
    tcp_connected_flag = 1;
    return ERR_OK;
}


void send(uint8_t *buf, const uint32_t body_len, const enum MessageType cmd) {
    struct nshead_t head = NSHEAD_DEFAULT;
    const uint32_t uid[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};
    const uint8_t sn[16] = {
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
    memcpy(head.sn, sn, sizeof(head.sn));
    head.checksum = body_len > 0 ? HAL_CRC_Calculate(&hcrc, (uint32_t *) buf, body_len) : 0;
    head.body_len = body_len;
    head.cmd = cmd;
    head.timestamp = DS3231_GetTimestamp();
    Fill_MessageID_With_HWRNG(head.message_id);
    const uint8_t *head_buffer = head_to_bytes(&head);
    tcp_write(tcp_client_pcb, head_buffer, sizeof(struct nshead_t), TCP_WRITE_FLAG_COPY);
    // 若有消息体，分片发送
    uint32_t sent = 0;
    while (sent < body_len) {
        const uint32_t CHUNK = 1472;
        const uint32_t remaining = body_len - sent;
        const uint32_t len = remaining > CHUNK ? CHUNK : remaining;
        // 等待发送缓冲区可用
        while (tcp_sndbuf(tcp_client_pcb) < len) {
            tcp_output(tcp_client_pcb);
            MX_LWIP_Process();
        }
        // 对中间分片添加 TCP_WRITE_FLAG_MORE，最后一片不加
        const uint8_t flags = TCP_WRITE_FLAG_COPY |
                              (sent + len < body_len ? TCP_WRITE_FLAG_MORE : 0);
        uint8_t data[len];
        for (int i = 0; i < len; ++i) {
            data[i] = buf[sent + i];
        }
        tcp_write(tcp_client_pcb, data, len, flags);
        sent += len;
    }
    // 最后一次 flush
    tcp_output(tcp_client_pcb);
}

/* 发生错误时的回调 */
void tcp_client_error(void *arg, err_t err) {
    /* 清理资源或重试 */
    tcp_client_pcb = NULL;
    printf("tcp_client_error\n");
    NVIC_SystemReset();
}
