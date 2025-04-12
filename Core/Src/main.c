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
float Temperature, Pressure, Humidity;
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

float Temperature, Pressure, Humidity;
uint32_t packet_seq = 0;
#define BUFFER_SIZE 64    // 定义接收缓冲区大小
uint8_t rxIndex = 0;
char rxBuffer[BUFFER_SIZE];
uint8_t uartReceiveByte;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // 存储接收到的字节
        if (rxIndex < BUFFER_SIZE - 1) {
            rxBuffer[rxIndex++] = uartReceiveByte;
            // 检查结束符
            if (uartReceiveByte == '\n' || uartReceiveByte == '\r') {
                rxBuffer[rxIndex - 1] = '\0'; // 替换结束符为字符串结束符
                // 处理命令
                if (strncmp(rxBuffer, "reboot", rxIndex) == 0) {
                    printf("Rebooting...\n");
                    NVIC_SystemReset();
                }
                if (strncmp(rxBuffer, "pressure", rxIndex) == 0) {
                    BME280_Measure();
                    printf("Pressure:%.0f\n", Pressure / 100);
                } else if (strncmp(rxBuffer, "time", rxIndex) == 0) {
                    DS3231_TimeType rtcTime;
                    DS3231_GetTime(&rtcTime);
                    printf("Time:20%02d-%02d-%02d %02d:%02d:%02d\n",
                           rtcTime.year, rtcTime.month, rtcTime.date,
                           rtcTime.hours, rtcTime.minutes, rtcTime.seconds);
                } else {
                    printf("%.*s\r\n", rxIndex, rxBuffer);
                }
                rxIndex = 0; // 重置索引
            }
        } else {
            printf("Buffer overflow. Clearing buffer.\n");
            memset(rxBuffer, 0, BUFFER_SIZE);
            rxIndex = 0;
        }
        // 继续接收下一个字节
        HAL_UART_Receive_IT(huart, &uartReceiveByte, 1);
    }
}

uint32_t CRC_Calculate(uint32_t mark, char text[], uint16_t len) {
    HAL_CRC_Calculate(&hcrc, &mark, 4);
    HAL_CRC_Accumulate(&hcrc, (uint32_t *) &len, 2);
    return HAL_CRC_Accumulate(&hcrc, (uint32_t *) text, len);
}

void read_all_eeprom(void) {
    printf("读取全部数据\n");
    uint8_t temp[4096];
    EEPROM_Read(0, temp, sizeof(temp));
    for (int i = 0; i < 4096; ++i) {
        printf("%02X ", temp[i]);
    }
    printf("\n");
}

void clear_all_eeprom(void) {
    printf("开始清除eeprom数据\n");
    const u_int8_t empty_data[4096] = {0x00};
    EEPROM_Write(0, empty_data, sizeof(empty_data));
}

void send(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, cJSON *data) {
    char *json_str = cJSON_PrintUnformatted(data);
    struct pbuf *udp_buffer = pbuf_alloc(PBUF_TRANSPORT, strlen(json_str), PBUF_RAM);
    if (udp_buffer != NULL) {
        memcpy(udp_buffer->payload, json_str, strlen(json_str));
        udp_sendto(pcb, udp_buffer, addr, port);
        pbuf_free(udp_buffer);
    }
    HAL_IWDG_Refresh(&hiwdg1);
    free(json_str);
    cJSON_Delete(data);
}

void packet_process(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port, cJSON *packet) {
    char sn[16];
    snprintf(sn, sizeof(sn), "%08X%08X%08X",
             HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
    const cJSON *sn_item = cJSON_GetObjectItemCaseSensitive(packet, "sn");
    const cJSON *packetSeq_item = cJSON_GetObjectItemCaseSensitive(packet, "packetSeq");
    if (!cJSON_IsNumber(packetSeq_item)) {
        return;
    }
    const uint32_t packetSeq = packetSeq_item->valueint;
    if (strcmp(sn, cJSON_GetStringValue(sn_item)) != 0) {
        return;
    }
    const cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(packet, "cmd");
    if (cJSON_IsString(cmd_item)) {
        const char *cmd = cJSON_GetStringValue(cmd_item);
        if (cmd != NULL) {
            if (strcmp(cmd, "reboot") == 0) {
                NVIC_SystemReset();
            }
            if (strcmp(cmd, "time") == 0) {
                DS3231_TimeType rtcTime;
                DS3231_GetTime(&rtcTime);
                printf("Time:20%02d-%02d-%02d %02d:%02d:%02d\n",
                       rtcTime.year, rtcTime.month, rtcTime.date,
                       rtcTime.hours, rtcTime.minutes, rtcTime.seconds);
            } else if (strcmp(cmd, "pong") == 0) {
                HAL_IWDG_Refresh(&hiwdg1);
            } else if (strcmp(cmd, "get_config") == 0) {
                uint32_t mark;
                EEPROM_Read(0, (uint8_t *) &mark, sizeof(mark));
                uint16_t len;
                EEPROM_Read(sizeof(mark), (uint8_t *) &len, sizeof(len));
                cJSON *data = cJSON_CreateObject();
                cJSON_AddNumberToObject(data, "packetSeq", ++packet_seq);
                cJSON_AddNumberToObject(data, "ackPacketSeq", packetSeq);
                cJSON_AddStringToObject(data, "cmd", "get_config_ack");
                cJSON_AddStringToObject(data, "sn", sn);
                if (0xFEFCDDDC != mark || len > 4096) {
                    cJSON_AddStringToObject(data, "data", "ERROR");
                } else {
                    u_int8_t text[len];
                    EEPROM_Read(sizeof(mark) + sizeof(len), (uint8_t *) &text, len);
                    uint16_t read_crc;
                    EEPROM_Read(sizeof(mark) + sizeof(len) + len, (uint8_t *) &read_crc, sizeof(read_crc));
                    uint32_t calculated_crc = CRC_Calculate(mark, text, len);
                    if (read_crc != calculated_crc) {
                        cJSON_AddStringToObject(data, "data", "CRC_ERROR");
                    } else {
                        cJSON_AddStringToObject(data, "data", text);
                    }
                }
                send(pcb, addr, port, data);
            } else if (strcmp(cmd, "set_config") == 0) {
                HAL_IWDG_Refresh(&hiwdg1);
                const cJSON *data_item = cJSON_GetObjectItemCaseSensitive(packet, "data");
                if (cJSON_IsString(data_item)) {
                    const char *text_str = cJSON_GetStringValue(data_item);
                    uint32_t mark = 0xFEFCDDDC;
                    uint16_t len = strlen(text_str) + 1;
                    // 分配内存，包含终止符
                    u_int8_t text[len];
                    memcpy(text, text_str, len); // 复制数据
                    uint32_t calculated_crc = CRC_Calculate(mark, text, len);
                    EEPROM_Write(0, (uint8_t *) &mark, sizeof(mark));
                    EEPROM_Write(sizeof(mark), (uint8_t *) &len, sizeof(len));
                    EEPROM_Write(sizeof(mark) + sizeof(len), (uint8_t *) &text, len);
                    EEPROM_Write(sizeof(mark) + sizeof(len) + len, (uint8_t *) &calculated_crc, sizeof(calculated_crc));
                }
                cJSON *data = cJSON_CreateObject();
                cJSON_AddNumberToObject(data, "packetSeq", ++packet_seq);
                cJSON_AddNumberToObject(data, "ackPacketSeq", packetSeq);
                cJSON_AddStringToObject(data, "cmd", "set_config_ack");
                cJSON_AddStringToObject(data, "data", "ok");
                cJSON_AddStringToObject(data, "sn", sn);
                HAL_IWDG_Refresh(&hiwdg1);
                send(pcb, addr, port, data);
            }
        }
    }
}

void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p == NULL) return;
    // 优化1: 使用固定缓冲区防止溢出
    char buffer[1473];
    // 优化2: 安全拷贝数据并添加终止符
    const u16_t len = pbuf_copy_partial(p, buffer, sizeof(buffer) - 1, 0);
    buffer[len > sizeof(buffer) - 1 ? sizeof(buffer) - 1 : len] = '\0';
    cJSON *packet = cJSON_Parse(buffer);
    pbuf_free(p);
    if (packet == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("JSON 解析错误 UDP[%s:%d] %.*s\n", ipaddr_ntoa(addr), port, (int) sizeof(buffer), buffer);
        }
        return;
    }
    packet_process(pcb, addr, port, packet);
    cJSON_Delete(packet);
}


void set_system_time(const time_t seconds, uint32_t us) {
    // 设置时区为 GMT+8
    setenv("TZ", "UTC-8", 1);
    tzset(); // 应用新的时区设置
    const struct tm *time_info = localtime(&seconds);
    DS3231_TimeType rtcTime;
    // 转换tm结构体到RTC时间格式
    rtcTime.seconds = time_info->tm_sec;
    rtcTime.minutes = time_info->tm_min;
    rtcTime.hours = time_info->tm_hour;
    rtcTime.day = time_info->tm_wday;
    rtcTime.date = time_info->tm_mday;
    rtcTime.month = time_info->tm_mon + 1;
    rtcTime.year = time_info->tm_year - 100;
    DS3231_SetTime(&rtcTime);
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

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
    /* USER CODE BEGIN 2 */
    HAL_UART_Receive_IT(&huart1, &uartReceiveByte, 1);
    if (HAL_OK != EEPROM_Init(&hi2c1)) {
        printf("初始化eeprom失败\n");
        NVIC_SystemReset();
    }
    DS3231_TimeType rtcTime;
    DS3231_GetTime(&rtcTime);
    printf("20%02d-%02d-%02d %02d:%02d:%02d\r\n",
           rtcTime.year, rtcTime.month, rtcTime.date,
           rtcTime.hours, rtcTime.minutes, rtcTime.seconds);
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

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
    sntp_setservername(0, "ntp.aliyun.com");
    BME280_Config(OSRS_2, OSRS_16, OSRS_1, MODE_NORMAL, T_SB_0p5, IIR_16);
    struct udp_pcb *udp_pcb = udp_new_ip6();
    if (udp_pcb) {
        if (udp_bind(udp_pcb, IP6_ADDR_ANY, 8668) == ERR_OK) {
            udp_recv(udp_pcb, udp_receive_callback, NULL);
        } else {
            printf("创建udp失败\n");
            udp_remove(udp_pcb);
            NVIC_SystemReset();
        }
    }
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        MX_LWIP_Process();
        for (int i = 0; i < 499999; ++i) {
            MX_LWIP_Process();
        }
        DS3231_GetTime(&rtcTime);
        BME280_Measure();
        cJSON *packet = cJSON_CreateObject();
        char sn_str[16];
        snprintf(sn_str, sizeof(sn_str), "%08X%08X%08X",
                 HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2());
        cJSON_AddStringToObject(packet, "sn", sn_str);
        const double temperatureRounded = round((double) Temperature * 10) / 10.0;
        const double humidityRounded = round((double) Humidity * 10) / 10.0;
        const int pressureRounded = (int) roundf(Pressure / 100);
        cJSON_AddNumberToObject(packet, "temperature", temperatureRounded);
        cJSON_AddNumberToObject(packet, "humidity", humidityRounded);
        cJSON_AddNumberToObject(packet, "pressure", pressureRounded);
        cJSON_AddNumberToObject(packet, "packetSeq", ++packet_seq);
        cJSON_AddStringToObject(packet, "cmd", "ping");
        char time_str[20];
        sprintf(time_str, "20%02d-%02d-%02d %02d:%02d:%02d",
                rtcTime.year, rtcTime.month, rtcTime.date,
                rtcTime.hours, rtcTime.minutes, rtcTime.seconds);
        cJSON_AddStringToObject(packet, "time", time_str);
        char *json_str = cJSON_PrintUnformatted(packet);
        struct pbuf *udp_buffer = pbuf_alloc(PBUF_TRANSPORT, strlen(json_str), PBUF_RAM);
        if (udp_buffer != NULL) {
            memcpy(udp_buffer->payload, json_str, strlen(json_str));
            udp_sendto(udp_pcb, udp_buffer, &api_ip, 8667);
            pbuf_free(udp_buffer);
        }
        cJSON_Delete(packet);
        free(json_str);
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
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
