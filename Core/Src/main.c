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
#include "fmc.h"

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
#include "sdram.h"
#include "shtc.h"
#include "sntp.h"
#include "tcp_client_services.h"
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

/* USER CODE BEGIN PFP */


#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(&huart1, (uint8_t *) &ch, 1, 0xFFFF);
    return ch;
}


uint8_t uartRxIndex = 0;
char uartRxBuffer[UART_RX_BUFFER_SIZE];
uint8_t uartReceiveByte;

uint8_t device_sn[16];


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


void set_system_time(time_t seconds, uint32_t us) {
    const time_t local_ts = seconds + 8 * 3600;
    struct tm time_info;
    gmtime_r(&local_ts, &time_info);
    DS3231_TimeType rtcTime;
    rtcTime.sec = time_info.tm_sec; // 0–59
    rtcTime.min = time_info.tm_min; // 0–59
    rtcTime.hour = time_info.tm_hour; // 0–23 (UTC)
    rtcTime.week = time_info.tm_wday; // 0–6
    rtcTime.day = time_info.tm_mday; // 1–31
    rtcTime.month = time_info.tm_mon + 1; // tm_mon: 0–11
    rtcTime.year = time_info.tm_year - 100; // since 1900
    if (HAL_OK == DS3231_SetTime(&rtcTime)) {
        printf("NTP 同步 RTC 成功\n");
    } else {
        printf("NTP 同步 RTC 失败，重启系统\n");
        NVIC_SystemReset();
    }
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
    MX_FMC_Init();
    /* USER CODE BEGIN 2 */
    HAL_UART_Receive_IT(&huart1, &uartReceiveByte, 1);
    if (HAL_OK != EEPROM_Init(&hi2c1)) {
        printf("初始化eeprom失败\n");
        NVIC_SystemReset();
    }
    DS3231_TimeType rtcTime;
    DS3231_GetTime(&rtcTime);
    printf("启动时间:%04d-%02d-%02d %02d:%02d:%02d\r\n",
           rtcTime.year, rtcTime.month, rtcTime.day,
           rtcTime.hour, rtcTime.min, rtcTime.sec);
    struct dhcp *dhcp;
    do {
        MX_LWIP_Process();
        dhcp = netif_dhcp_data(&gnetif);
    } while (dhcp == NULL || dhcp->state != DHCP_STATE_BOUND || !ip6_addr_isvalid(netif_ip6_addr_state(&gnetif, 1)));
    HAL_IWDG_Refresh(&hiwdg1);
    printf("IPv4 Address:%s\n", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    printf("IPv6 Address:%s \n", ip6addr_ntoa(netif_ip6_addr(&gnetif, 1)));
    //SDRAM_PerformanceTest();
    const ip_addr_t ntp_ip = get_ntp_ip();
    printf("NTP Server IP: %s\n", ipaddr_ntoa(&ntp_ip));
    const ip_addr_t api_ip = get_api_ip();
    printf("API Server IP: %s\n", ipaddr_ntoa(&api_ip));
    HAL_IWDG_Refresh(&hiwdg1);
    SHTC3_Init();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // 设置时区为 GMT+8
    // setenv("TZ", "UTC+8", 1);
    // tzset(); // 应用新的时区设置
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
