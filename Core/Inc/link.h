//
// Created by hemeng on 2025/4/13.
//

#ifndef LINK_H
#define LINK_H
#include "err.h"
#include <string.h>

#include "ip_addr.h"
#include "tcp.h"


/* 初始化 TCP 客户端 */
void tcp_client_init(ip_addr_t server_ip);

/* 错误回调 */
void tcp_client_error(void *arg, err_t err);

/* 连接成功回调 */
err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

/* 接收数据回调 */
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
#endif //LINK_H
