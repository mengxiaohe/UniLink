//
// Created by hemeng on 2025/4/13.
//

#include "link.h"


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
    uint16_t local_port = rnd % (65535u - 2000u + 1u) + 2000u;
    tcp_bind(tcp_client_pcb, &local_ip6, local_port);
    /* 2. 注册错误回调 */
    tcp_err(tcp_client_pcb, tcp_client_error);
    /* 4. 发起连接（三次握手） */
    tcp_connect(tcp_client_pcb,
                &server_ip,
                8033,
                tcp_client_connected);
}

/* 连接建立成功后的回调 */
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    const char *msg = "Hello from STM32!\r\n";
    if (err != ERR_OK) {
        return err;
    }
    /* 注册接收数据回调 */
    tcp_recv(tpcb, tcp_client_recv);
    /* 发送第一条消息 */
    tcp_write(tpcb, msg, strlen(msg), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    return ERR_OK;
}

/* 接收到服务器数据后的回调 */
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        /* 远端关闭了连接 */
        tcp_close(tpcb);
        return ERR_OK;
    }
    p->len;

    /* 回显接收到的数据 */
    tcp_write(tpcb, p->payload, p->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    /* 释放 pbuf */
    pbuf_free(p);
    return ERR_OK;
}

/* 发生错误时的回调 */
void tcp_client_error(void *arg, err_t err) {
    /* 清理资源或重试 */
    tcp_client_pcb = NULL;
}
