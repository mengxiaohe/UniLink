//
// Created by hemeng on 25-4-20.
//

#include "sdram.h"


/******************************************************************************************************
*	函 数 名: SDRAM_Initialization_Sequence
*	入口参数: hsdram - SDRAM_HandleTypeDef定义的变量，即表示定义的sdram
*				 Command	- 控制指令
*	返 回 值: 无
*	函数功能: SDRAM 参数配置
*	说    明: 配置SDRAM相关时序和控制方式
*******************************************************************************************************/

void SDRAM_InitConfig(SDRAM_HandleTypeDef *hsdram, FMC_SDRAM_CommandTypeDef *Command) {
    __IO uint32_t tmpmrd = 0;

    /* Configure a clock configuration enable command */
    Command->CommandMode = FMC_SDRAM_CMD_CLK_ENABLE; // 开启SDRAM时钟
    Command->CommandTarget = FMC_COMMAND_TARGET_BANK; // 选择要控制的区域
    Command->AutoRefreshNumber = 1;
    Command->ModeRegisterDefinition = 0;

    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT); // 发送控制指令
    HAL_Delay(1); // 延时等待

    /* Configure a PALL (precharge all) command */
    Command->CommandMode = FMC_SDRAM_CMD_PALL; // 预充电命令
    Command->CommandTarget = FMC_COMMAND_TARGET_BANK; // 选择要控制的区域
    Command->AutoRefreshNumber = 1;
    Command->ModeRegisterDefinition = 0;

    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT); // 发送控制指令

    /* Configure a Auto-Refresh command */
    Command->CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE; // 使用自动刷新
    Command->CommandTarget = FMC_COMMAND_TARGET_BANK; // 选择要控制的区域
    Command->AutoRefreshNumber = 8; // 自动刷新次数
    Command->ModeRegisterDefinition = 0;

    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT); // 发送控制指令

    /* Program the external memory mode register */
    tmpmrd = (uint32_t) SDRAM_MODEREG_BURST_LENGTH_2 |
             SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
             SDRAM_MODEREG_CAS_LATENCY_3 |
             SDRAM_MODEREG_OPERATING_MODE_STANDARD |
             SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    Command->CommandMode = FMC_SDRAM_CMD_LOAD_MODE; // 加载模式寄存器命令
    Command->CommandTarget = FMC_COMMAND_TARGET_BANK; // 选择要控制的区域
    Command->AutoRefreshNumber = 1;
    Command->ModeRegisterDefinition = tmpmrd;

    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT); // 发送控制指令

    HAL_SDRAM_ProgramRefreshRate(hsdram, 918); // 配置刷新率
}
/******************************************************************************************************
*	函 数 名: SDRAM_Test
*	入口参数: 无
*	返 回 值: SUCCESS - 成功，ERROR - 失败
*	函数功能: SDRAM测试
*	说    明: 先以16位的数据宽度写入数据，再读取出来一一进行比较，随后以8位的数据宽度写入，
*				 用以验证NBL0和NBL1两个引脚的连接是否正常。
*******************************************************************************************************/

uint8_t SDRAM_PerformanceTest(void) {
    uint32_t i = 0;                    // 循环计数器
    uint16_t readData = 0;             // 16 位宽度读取数据
    uint8_t readData8b = 0;            // 8 位宽度读取数据

    uint32_t startTime = 0;            // 开始时间
    uint32_t endTime = 0;              // 结束时间
    uint32_t elapsedTime = 0;          // 执行时间（单位：毫秒）
    float speedMBps = 0.0f;            // 执行速度（单位：MB/s）

    printf("***************************************************************\n");
    printf("开始 SDRAM 性能测试...\n");

    // 写入数据（16 位宽度）
    printf(">> 正在以 16 位宽度写入数据...\n");
    startTime = HAL_GetTick();
    for (i = 0; i < SDRAM_Size / 2; i++) {
        *(__IO uint16_t *)(SDRAM_BANK_ADDR + 2 * i) = (uint16_t)i;
    }
    endTime = HAL_GetTick();
    elapsedTime = endTime - startTime;
    speedMBps = ((float)SDRAM_Size / 1024 / 1024) / elapsedTime * 1000;
    printf("16 位写入完成，总大小：%d MB，耗时：%d 毫秒，速度：%.2f MB/s\n", SDRAM_Size / 1024 / 1024, elapsedTime, speedMBps);

    // 读取数据（16 位宽度）
    printf(">> 正在以 16 位宽度读取数据...\n");
    startTime = HAL_GetTick();
    for (i = 0; i < SDRAM_Size / 2; i++) {
        readData = *(__IO uint16_t *)(SDRAM_BANK_ADDR + 2 * i);
    }
    endTime = HAL_GetTick();
    elapsedTime = endTime - startTime;
    speedMBps = ((float)SDRAM_Size / 1024 / 1024) / elapsedTime * 1000;
    printf("16 位读取完成，总大小：%d MB，耗时：%d 毫秒，速度：%.2f MB/s\n", SDRAM_Size / 1024 / 1024, elapsedTime, speedMBps);

    // 校验数据（16 位宽度）
    printf(">> 正在校验 16 位宽度数据...\n");
    for (i = 0; i < SDRAM_Size / 2; i++) {
        readData = *(__IO uint16_t *)(SDRAM_BANK_ADDR + 2 * i);
        if (readData != (uint16_t)i) {
            printf("16 位数据校验失败！索引：%lu，读取值：%u，预期值：%u\n", i, readData, (uint16_t)i);
            return ERROR;
        }
    }

    // 写入和校验数据（8 位宽度）
    printf("16 位宽度数据测试通过，开始 8 位宽度数据测试...\n");
    for (i = 0; i < 255; i++) {
        *(__IO uint8_t *)(SDRAM_BANK_ADDR + i) = (uint8_t)i;
    }
    printf("8 位数据写入完成，正在校验...\n");
    for (i = 0; i < 255; i++) {
        readData8b = *(__IO uint8_t *)(SDRAM_BANK_ADDR + i);
        if (readData8b != (uint8_t)i) {
            printf("8 位数据校验失败！索引：%lu，读取值：%u，预期值：%u\n", i, readData8b, (uint8_t)i);
            printf("请检查 NBL0 和 NBL1 的连接是否正常。\n");
            return ERROR;
        }
    }

    printf("8 位宽度数据测试通过。\n");
    printf("SDRAM 测试通过，系统运行正常！\n");
    return SUCCESS;
}
