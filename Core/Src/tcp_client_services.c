//
// Created by hemeng on 25-4-19.
//

#include "tcp_client_services.h"

#include "iwdg.h"


extern char sn[16];
extern uint32_t Pressure;
uint32_t lanRxIndex;

extern CRC_HandleTypeDef hcrc;

uint8_t packet_buffer[LAN_RX_BUFFER_SIZE] __attribute__((section(".packet_buffer"))) = {0};

/* 接收到服务器数据后的回调 */
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (err != ERR_OK || p == NULL) {
        tcp_connected_flag = 0;
        tcp_close(tpcb);
        NVIC_SystemReset();
    }
    struct pbuf *q = p;
    while (q != NULL) {
        if (lanRxIndex + q->len > LAN_RX_BUFFER_SIZE) {
            printf("超过缓冲区容量\n");
            pbuf_free(p);
            NVIC_SystemReset();
        }
        uint8_t *data = q->payload;
        for (int i = 0; i < q->len; ++i) {
            packet_buffer[lanRxIndex + i] = data[i];
        }
        lanRxIndex += q->len;
        q = q->next;
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}


void heartbeat_handler() {
    uint8_t body[0];
    send(body, 0, PING);
}

void upload_big_data_handler() {
    uint8_t body[2048];
    send(body, 2048, UPLOAD_BIG_DATA);
}


uint8_t ping_flag = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        ping_flag = 1;
    }
}

void print_message_id(uint8_t message_id[16]) {
    for (int i = 0; i < 16; i++) {
        printf("%02X", message_id[i]);
    }
    printf("\r\n");
}

void process_data() {
    HAL_IWDG_Refresh(&hiwdg1);
    if (tcp_connected_flag && ping_flag) {
        ping_flag = 0;
        heartbeat_handler();
        upload_big_data_handler();
    }
    if (lanRxIndex >= sizeof(struct nshead_t)) {
        struct nshead_t nshead = bytes_to_head(packet_buffer);
        if (NSHEAD_MAGICNUM != nshead.magic_num) {
            printf("magic错误\n");
            NVIC_SystemReset();
        }
        const uint32_t body_len = nshead.body_len;
        const uint32_t msg_len = sizeof(struct nshead_t) + body_len;
        if (msg_len > lanRxIndex) {
            return;
        }
        lanRxIndex -= msg_len;
        if (body_len > 0) {
            uint8_t *src = packet_buffer + sizeof(struct nshead_t);
            const uint32_t actual_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *) src, body_len);
            if (actual_crc != nshead.checksum) {
                printf("crc error  actual_crc:%x checksum:%x body_len:%d\n", actual_crc, nshead.checksum, body_len);
                NVIC_SystemReset();
            }
            if (body_len > 1024) {
                printf("消息解析结束 len:%d timestamp:%d cmd:%04X checksum:%04X  \n", msg_len, nshead.timestamp, nshead.cmd,
                       nshead.checksum);
            } else {
                printf("消息解析结束 len:%d timestamp:%d cmd:%04X checksum:%04X body:%.*s\n", msg_len, nshead.timestamp,
                       nshead.cmd,
                       nshead.checksum, body_len, (char *) src);
            }
        }
        if (nshead.cmd == PONG) {
        } else if (nshead.cmd == DOWNLOAD_BIG_DATA) {
            send(nshead.message_id, 16, TERMINAL_UNIVERSAL_ACK);
        }
        memmove(&packet_buffer[0], &packet_buffer[msg_len], lanRxIndex * sizeof(packet_buffer[0]));
        memset(&packet_buffer[lanRxIndex], 0, (LAN_RX_BUFFER_SIZE - lanRxIndex) * sizeof(packet_buffer[0]));
    }
}
