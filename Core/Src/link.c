//
// Created by hemeng on 2025/4/13.
//

#include "link.h"

#include "cJSON.h"
#include "crc.h"


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

//htons   htonl
uint8_t *struct_to_bytes(const struct nshead_t *head) {
    static uint8_t buffer[16];
    const uint32_t magic_num_net = htonl(head->magic_num);
    memcpy(buffer, &magic_num_net, 4);
    const uint16_t version_net = htons(head->version);
    memcpy(buffer + 4, &version_net, 2);
    const uint32_t reserved_net = htonl(head->reserved);
    memcpy(buffer + 6, &reserved_net, 4);
    const uint32_t body_len_net = htonl(head->body_len);
    memcpy(buffer + 10, &body_len_net, 4);
    const uint16_t checksum_net = htons(head->checksum);
    memcpy(buffer + 14, &checksum_net, 2);
    return buffer;
}

struct nshead_t bytes_to_struct(const uint8_t *buffer) {
    struct nshead_t head;
    uint32_t magic_num_net;
    memcpy(&magic_num_net, buffer, 4);
    head.magic_num = ntohl(magic_num_net);
    uint16_t version_net;
    memcpy(&version_net, buffer + 4, 2);
    head.version = ntohs(version_net);
    uint32_t reserved_net;
    memcpy(&reserved_net, buffer + 6, 4);
    head.reserved = ntohl(reserved_net);
    uint32_t body_len_net;
    memcpy(&body_len_net, buffer + 10, 4);
    head.body_len = ntohl(body_len_net);
    uint16_t checksum_net;
    memcpy(&checksum_net, buffer + 14, 2);
    head.checksum = ntohs(checksum_net);
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


void send(char *buf) {
    const size_t body_len = strlen(buf);
    struct nshead_t head = NSHEAD_DEFAULT;
    head.checksum = HAL_CRC_Calculate(&hcrc, (uint32_t *) buf, body_len);
    head.body_len = body_len;
    const uint8_t *head_buffer = struct_to_bytes(&head);
    tcp_write(tcp_client_pcb, head_buffer, sizeof(struct nshead_t), TCP_WRITE_FLAG_COPY);
    tcp_write(tcp_client_pcb, buf, strlen(buf), TCP_WRITE_FLAG_COPY);
    tcp_output(tcp_client_pcb);
}

/* 发生错误时的回调 */
void tcp_client_error(void *arg, err_t err) {
    /* 清理资源或重试 */
    tcp_client_pcb = NULL;
    printf("tcp_client_error\n");
    NVIC_SystemReset();
}
