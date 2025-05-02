//
// Created by hemeng on 25-4-19.
//

#include "tcp_client_services.h"

#include "iwdg.h"
#include "memory_sections.h"


uint32_t lanRxIndex = 0;

extern CRC_HandleTypeDef hcrc;

extern uint8_t device_sn[16];

uint8_t ack_flag;
uint8_t ota_flag;

uint8_t ack_message_id[16];
/* 接收到服务器数据后的回调 */
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (err != ERR_OK || p == NULL) {
        tcp_connected_flag = 0;
        tcp_close(tpcb);
        printf("收到关闭请求\n");
        NVIC_SystemReset();
    }
    const struct pbuf *q = p;
    while (q != NULL) {
        if (lanRxIndex + q->len > LAN_PACKET_RX_BUFFER_SIZE) {
            printf("超过缓冲区容量\n");
            pbuf_free(p);
            NVIC_SystemReset();
        }
        const uint8_t *data = q->payload;
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
    const uint32_t body_len = 1024 * 1024 * 2;
    send(unallocated_memory, body_len, UPLOAD_BIG_DATA);
}
#pragma pack(push, 1)

typedef struct {
    uint16_t temperature; /* 温度 */
    uint16_t humidity; /* 湿度 */
    uint32_t pressure; /* 气压 */
} sensor_data_t;
#pragma pack(pop)

void upload_status_handler() {
    uint16_t temperature;
    uint16_t humidity;
    float pressure;
    SHTC3_GetTempAndHumi(&temperature, &humidity);
    BME280_Measure(NULL,NULL, &pressure);
    const sensor_data_t data = {
        .temperature = htons(temperature), /* 比如表示 25.0°C */
        .humidity = htons(humidity), /* 比如表示 60.0% */
        .pressure = htonl((uint32_t)pressure) /* 单位 Pa */
    };
    uint8_t buf[sizeof(sensor_data_t)];
    memcpy(buf, &data, sizeof(buf));
    send(buf, sizeof(sensor_data_t), UPLOAD_STATUS);
}

uint64_t uw_tick = 0;
uint8_t heartbeat_flag = 0;
uint8_t upload_big_data_flag = 0;
uint8_t upload_status_flag = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        uw_tick += 1;
        if (uw_tick % 1000 == 0) {
            heartbeat_flag = 1;
        }
        if (uw_tick % 6000 == 0) {
            upload_big_data_flag = 1;
        }
        if (uw_tick % 100 == 0) {
            upload_status_flag = 1;
        }
    }
}

void print_message_id(uint8_t message_id[16]) {
    for (int i = 0; i < 16; i++) {
        printf("%02X", message_id[i]);
    }
    printf("\r\n");
}

typedef void (*pFunction)(void);

// 跳转方法一定要放到main中
void Jump_To_App(uint32_t addr) {
    printf("bootloader to app\r\n");
    __disable_irq();
    uint32_t JumpAddress;
    JumpAddress = *(volatile uint32_t *) (addr + 4);
    pFunction Jump_To_Application;
    Jump_To_Application = (pFunction) JumpAddress;
    __set_MSP(*(volatile uint32_t *) addr);
    Jump_To_Application();
}

void process_data() {
    HAL_IWDG_Refresh(&hiwdg1);
    if (lanRxIndex >= sizeof(struct nshead_t)) {
        struct nshead_t nshead = bytes_to_head(packet_buffer);
        if (NSHEAD_MAGICNUM != nshead.magic_num) {
            printf("magic错误\n");
            NVIC_SystemReset();
        }
        const uint32_t body_len = nshead.body_len;
        const uint32_t msg_len = sizeof(struct nshead_t) + body_len;
        if (msg_len <= lanRxIndex) {
            if (nshead.cmd == PONG) {
            } else if (nshead.cmd == DOWNLOAD_BIG_DATA) {
                for (int i = 0; i < 16; ++i) {
                    ack_message_id[i] = nshead.message_id[i];
                }
                ack_flag = 1;
            } else if (nshead.cmd == OTA) {
                for (int i = 0; i < 16; ++i) {
                    ack_message_id[i] = nshead.message_id[i];
                }
                ota_flag = 1;
            }
            if (body_len > 0) {
                uint8_t *src = packet_buffer + sizeof(struct nshead_t);
                const uint32_t actual_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *) src, body_len);
                if (actual_crc != nshead.checksum) {
                    printf("crc error  actual_crc:%x checksum:%x body_len:%d\n", actual_crc, nshead.checksum, body_len);
                    NVIC_SystemReset();
                }
                if (body_len > 1024) {
                    printf("消息解析结束 len:%d timestamp:%d cmd:%04X checksum:%04X  \n", msg_len, nshead.timestamp,
                           nshead.cmd,
                           nshead.checksum);
                } else {
                    printf("消息解析结束 len:%d timestamp:%d cmd:%04X checksum:%04X body:%.*s\n", msg_len, nshead.timestamp,
                           nshead.cmd,
                           nshead.checksum, body_len, (char *) src);
                }
                if (nshead.cmd == OTA) {
                    ota_flag = 1;
                    for (int i = 0; i < body_len; ++i) {
                        code_buffer[i] = src[i];
                    }
                    Jump_To_App(0x08100000);
                }
            }
            lanRxIndex -= msg_len;
            memmove(&packet_buffer[0], &packet_buffer[msg_len], lanRxIndex * sizeof(packet_buffer[0]));
            memset(&packet_buffer[lanRxIndex], 0, (LAN_PACKET_RX_BUFFER_SIZE - lanRxIndex) * sizeof(packet_buffer[0]));
        }
    }
    if (tcp_connected_flag && heartbeat_flag) {
        heartbeat_flag = 0;
        heartbeat_handler();
    }
    if (tcp_connected_flag && upload_big_data_flag) {
        upload_big_data_flag = 0;
        upload_big_data_handler();
    }
    if (tcp_connected_flag && upload_status_flag) {
        upload_status_flag = 0;
        upload_status_handler();
    }
    if (tcp_connected_flag && ack_flag) {
        ack_flag = 0;
        send(ack_message_id, 16, TERMINAL_UNIVERSAL_ACK);
    }
    if (tcp_connected_flag && ota_flag) {
        ota_flag = 0;
        send(ack_message_id, 16, TERMINAL_UNIVERSAL_ACK);
    }
}
