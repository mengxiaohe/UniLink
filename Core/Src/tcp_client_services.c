//
// Created by hemeng on 25-4-19.
//

#include "tcp_client_services.h"


extern char sn[16];
extern uint32_t Pressure;
extern uint32_t lanRxIndex;
extern uint8_t lanRxBuffer[LAN_RX_BUFFER_SIZE];;
extern CRC_HandleTypeDef hcrc;
/* 接收到服务器数据后的回调 */
err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (err != ERR_OK || p == NULL) {
        tcp_connected_flag = 0;
        tcp_close(tpcb);
        NVIC_SystemReset();
    }
    /* 访问接收到的数据 */
    const uint8_t *data = p->payload;
    const uint16_t len = p->len;
    memmove(lanRxBuffer + lanRxIndex, data, len);
    lanRxIndex += len;
    if (LAN_RX_BUFFER_SIZE < lanRxIndex) {
        printf("超过缓冲区\n");
        NVIC_SystemReset();
    }
    /* 通知 lwIP 已经接收 len 字节 */
    tcp_recved(tpcb, len);
    /* 释放 pbuf */
    pbuf_free(p);
    return ERR_OK;
}

void heartbeat_handler() {
    cJSON *packet = cJSON_CreateObject();
    cJSON_AddStringToObject(packet, "sn", sn);
    cJSON_AddStringToObject(packet, "cmd", "ping");
    char time_str[20];
    DS3231_TimeType rtcTime;
    DS3231_GetTime(&rtcTime);
    sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d",
            rtcTime.year, rtcTime.moon, rtcTime.day,
            rtcTime.hour, rtcTime.min, rtcTime.sec);
    cJSON_AddStringToObject(packet, "time", time_str);
    uint16_t temp, humi;
    if (HAL_OK != SHTC3_Wakeup() || HAL_OK != SHTC3_GetTempAndHumi(&temp, &humi)) {
        temp = 0, humi = 0;
    }
    DS3231_GetTime(&rtcTime);
    BME280_Measure();
    cJSON_AddNumberToObject(packet, "temperature", temp);
    cJSON_AddNumberToObject(packet, "humidity", humi);
    cJSON_AddNumberToObject(packet, "pressure", Pressure);
    cJSON *di_array = cJSON_CreateArray();
    cJSON *di_item = cJSON_CreateObject();
    cJSON_AddNumberToObject(di_item, "pin", 0);
    GPIO_PinState gpio0PinState = HAL_GPIO_ReadPin(GPIOI,GPIO_PIN_8);
    cJSON_AddStringToObject(di_item, "status", GPIO_PIN_RESET == gpio0PinState ? "ON" : "OFF");
    cJSON_AddItemToArray(di_array, di_item);
    cJSON_AddItemToObject(packet, "do", di_array);
    char *packet_str = cJSON_PrintUnformatted(packet);
    send(packet_str);
    cJSON_Delete(packet);
    free(packet_str);
}

void ack(const char *cmd) {
    cJSON *packet = cJSON_CreateObject();
    cJSON_AddStringToObject(packet, "sn", sn);
    cJSON_AddStringToObject(packet, "cmd", cmd);
    char time_str[20];
    DS3231_TimeType rtcTime;
    DS3231_GetTime(&rtcTime);
    sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d",
            rtcTime.year, rtcTime.moon, rtcTime.day,
            rtcTime.hour, rtcTime.min, rtcTime.sec);
    cJSON_AddStringToObject(packet, "time", time_str);
    char *packet_str = cJSON_PrintUnformatted(packet);
    send(packet_str);
    cJSON_Delete(packet);
    free(packet_str);
}

void read_config() {
    cJSON *packet = cJSON_CreateObject();
    cJSON_AddStringToObject(packet, "sn", sn);
    cJSON_AddStringToObject(packet, "cmd", "read_config_ack");
    char time_str[20];
    DS3231_TimeType rtcTime;
    DS3231_GetTime(&rtcTime);
    sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d",
            rtcTime.year, rtcTime.moon, rtcTime.day,
            rtcTime.hour, rtcTime.min, rtcTime.sec);
    cJSON_AddStringToObject(packet, "time", time_str);
    uint32_t mark;
    EEPROM_Read(0, (uint8_t *) &mark, sizeof(mark));
    uint16_t len;
    EEPROM_Read(sizeof(mark), (uint8_t *) &len, sizeof(len));
    if (0xFEFCDDDC != mark || len > 4096) {
        cJSON_AddStringToObject(packet, "data", "EEPROM ERROR");
    } else {
        u_int8_t text[len];
        EEPROM_Read(sizeof(mark) + sizeof(len), (uint8_t *) &text, len);
        uint16_t read_crc;
        EEPROM_Read(sizeof(mark) + sizeof(len) + len, (uint8_t *) &read_crc, sizeof(read_crc));
        uint16_t calculated_crc = CRC_Calculate(mark, text, len);
        if (read_crc != calculated_crc) {
            cJSON_AddStringToObject(packet, "data", "EEPROM CRC ERROR");
        } else {
            cJSON_AddStringToObject(packet, "data", text);
        }
    }
    char *packet_str = cJSON_PrintUnformatted(packet);
    send(packet_str);
    cJSON_Delete(packet);
    free(packet_str);
}

uint8_t ping_flag = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        ping_flag = 1;
    }
}

void process_data() {
    if (tcp_connected_flag) {
        if (ping_flag == 1) {
            ping_flag = 0;
            heartbeat_handler();
        }
    }
    if (lanRxIndex >= sizeof(struct nshead_t)) {
        struct nshead_t nshead = bytes_to_struct(lanRxBuffer);
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
        uint8_t *src = lanRxBuffer + sizeof(struct nshead_t);
        const uint32_t actual_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *) src, body_len);
        if (actual_crc != nshead.checksum) {
            printf("crc error  actual_crc:%x checksum:%x body_len:%d\n", actual_crc, nshead.checksum, body_len);
            NVIC_SystemReset();
        }
        // cJSON *packet = cJSON_Parse((const char *) src);
        // if (packet == NULL) {
        //     NVIC_SystemReset();
        // }
        memmove(&lanRxBuffer[0], &lanRxBuffer[msg_len], lanRxIndex * sizeof(lanRxBuffer[0]));
        memset(&lanRxBuffer[lanRxIndex], 0, (LAN_RX_BUFFER_SIZE - lanRxIndex) * sizeof(lanRxBuffer[0]));
        // const cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(packet, "cmd");
        // const cJSON *sn_item = cJSON_GetObjectItemCaseSensitive(packet, "sn");
        // if (!cJSON_IsString(cmd_item) || !cJSON_IsString(sn_item)) {
        //     NVIC_SystemReset();
        // }
        // if (strcmp(cJSON_GetStringValue(sn_item), sn) != 0) {
        //     NVIC_SystemReset();
        // }
        // const char *cmd = cJSON_GetStringValue(cmd_item);
        // if (strcmp(cmd, "pong") == 0) {
        // } else if (strcmp(cmd, "led0_on") == 0) {
        //     HAL_GPIO_WritePin(GPIOI,GPIO_PIN_8, GPIO_PIN_RESET);
        //     ack("led0_on_ack");
        // } else if (strcmp(cmd, "led0_off") == 0) {
        //     HAL_GPIO_WritePin(GPIOI,GPIO_PIN_8, GPIO_PIN_SET);
        //     ack("led0_off_ack");
        // } else if (strcmp(cmd, "test") == 0) {
        //     ack("led0_off_ack");
        // } else if (strcmp(cmd, "read_config") == 0) {
        //     read_config();
        // } else if (strcmp(cmd, "reboot") == 0) {
        //     printf("收到重启命令\n");
        //     NVIC_SystemReset();
        // } else if (strcmp(cmd, "ota") == 0) {
        //     ack("ota_ack");
        //     printf("msg  checksum:%x len:%d\n", nshead.checksum, nshead.body_len);
        // } else {
        //     char *json_str = cJSON_PrintUnformatted(packet);
        //     //printf("%s\n", json_str);
        //     free(json_str);
        // }
        // cJSON_Delete(packet);
    }
}
