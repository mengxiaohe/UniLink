/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "crc.h"
#include "dma.h"
#include "i2c.h"
#include "iwdg.h"
#include "lwip.h"
#include "memorymap.h"
#include "rng.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <string.h>

#include "at24c32.h"
#include "bme280.h"
#include "cJSON.h"
#include "crc16_modbus.h"
#include "dns_resolver.h"
#include "ds3231.h"
#include "link.h"
#include "shtc.h"
#include "sntp.h"
#include "prot/dhcp.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
extern struct netif gnetif;


uint8_t tcp_connected_flag = 0;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

static void MPU_Config(void);

/* USER CODE BEGIN PFP */


#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(&huart1, (uint8_t *) &ch, 1, 0xFFFF);
    return ch;
}

float Temperature;
float Humidity;
uint32_t Pressure;


#define UART_RX_BUFFER_SIZE 64
uint8_t uartRxIndex = 0;
char uartRxBuffer[UART_RX_BUFFER_SIZE];
uint8_t uartReceiveByte;

char sn[16];
#define LAN_RX_BUFFER_SIZE 40960
uint8_t lanRxIndex = 0;
uint8_t lanRxBuffer[LAN_RX_BUFFER_SIZE];


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_10) {
        printf("EXTI15_10 Triggered!\n");
        DS3231_StatusReg_t ds3231_status_reg;
        DS3231_ReadStatusReg(&ds3231_status_reg);
        ds3231_status_reg.bits.A1F = 0;
        while (HAL_OK != DS3231_WriteStatusReg(&ds3231_status_reg)) {
            HAL_Delay(5);
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // 存储接收到的字节
        if (uartRxIndex < UART_RX_BUFFER_SIZE - 1) {
            uartRxBuffer[uartRxIndex++] = uartReceiveByte;
            // 检查结束符
            if (uartReceiveByte == '\n' || uartReceiveByte == '\r') {
                uartRxBuffer[uartRxIndex - 1] = '\0'; // 替换结束符为字符串结束符
                // 处理命令
                if (strncmp(uartRxBuffer, "reboot", uartRxIndex) == 0) {
                    printf("Rebooting...\n");
                    NVIC_SystemReset();
                }
                if (strncmp(uartRxBuffer, "pressure", uartRxIndex) == 0) {
                    BME280_Measure();
                    printf("Pressure:%.0f\n", Pressure / 100.0);
                } else if (strncmp(uartRxBuffer, "time", uartRxIndex) == 0) {
                    DS3231_TimeType rtcTime;
                    DS3231_GetTime(&rtcTime);
                    printf("Time:20%02d-%02d-%02d %02d:%02d:%02d\n",
                           rtcTime.year, rtcTime.moon, rtcTime.day,
                           rtcTime.hour, rtcTime.min, rtcTime.sec);
                } else {
                    printf("%.*s\r\n", uartRxIndex, uartRxBuffer);
                }
                uartRxIndex = 0; // 重置索引
            }
        } else {
            printf("Buffer overflow. Clearing buffer.\n");
            memset(uartRxBuffer, 0, UART_RX_BUFFER_SIZE);
            uartRxIndex = 0;
        }
        // 继续接收下一个字节
        HAL_UART_Receive_IT(huart, &uartReceiveByte, 1);
    }
}


void set_system_time(const time_t seconds, uint32_t us) {
    // 设置时区为 GMT+8
    setenv("TZ", "UTC-8", 1);
    tzset(); // 应用新的时区设置
    const struct tm *time_info = localtime(&seconds);
    DS3231_TimeType rtcTime;
    // 转换tm结构体到RTC时间格式
    rtcTime.sec = time_info->tm_sec;
    rtcTime.min = time_info->tm_min;
    rtcTime.hour = time_info->tm_hour;
    rtcTime.week = time_info->tm_wday;
    rtcTime.day = time_info->tm_mday;
    rtcTime.moon = time_info->tm_mon + 1;
    rtcTime.year = time_info->tm_year - 100;
    if (HAL_OK == DS3231_SetTime(&rtcTime)) {
        printf("ntp同步时间成功\n");
    } else {
        printf("ntp同步时间失败\n");
        NVIC_SystemReset();
    }
}


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

uint8_t ping_flag = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        ping_flag = 1;
    }
}


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

void ping() {
    if (ping_flag == 1) {
        ping_flag = 0;
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
        char *packet_str = cJSON_PrintUnformatted(packet);
        send(packet_str);
        cJSON_Delete(packet);
        free(packet_str);
    }
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

void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]); // 大写字母，空格分隔
    }
    printf("\n");
}

void process_data() {
    if (lanRxIndex >= sizeof(struct nshead_t)) {
        struct nshead_t nshead = bytes_to_struct(lanRxBuffer);
        if (NSHEAD_MAGICNUM != nshead.magic_num) {
            printf("magic错误\n");
            NVIC_SystemReset();
        }
        const uint16_t body_len = nshead.body_len;
        const uint32_t msg_len = sizeof(struct nshead_t) + body_len;
        if (msg_len > lanRxIndex) {
            return;
        }
        lanRxIndex -= msg_len;
        const uint8_t *src = lanRxBuffer + sizeof(struct nshead_t);
        uint8_t temp[body_len];
        memcpy(temp, src, body_len);
        const uint32_t actual_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *) temp, body_len);
        if (actual_crc != nshead.checksum) {
            printf("crc error\n");
            NVIC_SystemReset();
        }
        cJSON *packet = cJSON_Parse(temp);
        if (packet == NULL) {
            NVIC_SystemReset();
        }
        memmove(&lanRxBuffer[0], &lanRxBuffer[msg_len], lanRxIndex * sizeof(lanRxBuffer[0]));
        memset(&lanRxBuffer[lanRxIndex], 0, (LAN_RX_BUFFER_SIZE - lanRxIndex) * sizeof(lanRxBuffer[0]));
        const cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(packet, "cmd");
        const cJSON *sn_item = cJSON_GetObjectItemCaseSensitive(packet, "sn");
        if (!cJSON_IsString(cmd_item) || !cJSON_IsString(sn_item)) {
            NVIC_SystemReset();
        }
        if (strcmp(cJSON_GetStringValue(sn_item), sn) != 0) {
            NVIC_SystemReset();
        }
        const char *cmd = cJSON_GetStringValue(cmd_item);
        if (strcmp(cmd, "pong") == 0) {
        } else if (strcmp(cmd, "led0_on") == 0) {
            HAL_GPIO_WritePin(GPIOI,GPIO_PIN_8, GPIO_PIN_RESET);
            ack("led0_on_ack");
        } else if (strcmp(cmd, "led0_off") == 0) {
            HAL_GPIO_WritePin(GPIOI,GPIO_PIN_8, GPIO_PIN_SET);
            ack("led0_off_ack");
        } else if (strcmp(cmd, "test") == 0) {
            ack("led0_off_ack");
        } else if (strcmp(cmd, "read_config") == 0) {
            read_config();
        } else {
            char *json_str = cJSON_PrintUnformatted(packet);
            printf("%s\n", json_str);
            free(json_str);
        }
        cJSON_Delete(packet);
    }
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MPU Configuration--------------------------------------------------------*/
    MPU_Config();

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */


    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_I2C1_Init();
    MX_LWIP_Init();
    MX_IWDG1_Init();
    MX_CRC_Init();
    MX_I2C3_Init();
    MX_RNG_Init();
    MX_TIM2_Init();
    /* USER CODE BEGIN 2 */
    snprintf(sn, 16, "%08X%08X%08X",
             HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
    HAL_UART_Receive_IT(&huart1, &uartReceiveByte, 1);
    if (HAL_OK != EEPROM_Init(&hi2c1)) {
        printf("初始化eeprom失败\n");
        NVIC_SystemReset();
    }
    DS3231_TimeType rtcTime;
    DS3231_GetTime(&rtcTime);
    printf("启动时间:%04d-%02d-%02d %02d:%02d:%02d\r\n",
           rtcTime.year, rtcTime.moon, rtcTime.day,
           rtcTime.hour, rtcTime.min, rtcTime.sec);
    struct dhcp *dhcp;
    do {
        MX_LWIP_Process();
        dhcp = netif_dhcp_data(&gnetif);
    } while (dhcp == NULL || dhcp->state != DHCP_STATE_BOUND || !ip6_addr_isvalid(netif_ip6_addr_state(&gnetif, 1)));
    HAL_IWDG_Refresh(&hiwdg1);
    printf("IPv4 Address:%s\n", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    printf("IPv6 Address:%s \n", ip6addr_ntoa(netif_ip6_addr(&gnetif, 1)));
    const ip_addr_t ntp_ip = get_ntp_ip();
    printf("NTP Server IP: %s\n", ipaddr_ntoa(&ntp_ip));
    const ip_addr_t api_ip = get_api_ip();
    printf("API Server IP: %s\n", ipaddr_ntoa(&api_ip));
    HAL_IWDG_Refresh(&hiwdg1);
    SHTC3_Init();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
    sntp_setservername(0, "ntp.aliyun.com");
    BME280_Config(OSRS_2, OSRS_16, OSRS_1, MODE_NORMAL, T_SB_0p5, IIR_16);
    tcp_client_init(api_ip);
    HAL_TIM_Base_Start_IT(&htim2);

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        MX_LWIP_Process();
        HAL_IWDG_Refresh(&hiwdg1);
        if (tcp_connected_flag) {
            ping();
        }
        process_data();
    }
    /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Supply configuration update enable
    */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /** Configure the main internal regulator output voltage
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSI
                                       | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 60;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                  | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    /* Disables the MPU */
    HAL_MPU_Disable();

    /** Initializes and configures the Region and the memory to be protected
    */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x0;
    MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable = 0x87;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    /* Enables the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
