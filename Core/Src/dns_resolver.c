//
// Created by hemeng on 2025/4/1.
//
#include <dns_resolver.h>



void dns_query_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    /* 提前处理失败和无效参数情况 */
    if (ipaddr == NULL) {
        printf("DNS lookup failed: %s\n", name);
        return;
    }
    printf("DNS success: %s -> %s\n", name, ipaddr_ntoa(ipaddr));
    /* 添加类型安全检查和空指针防护 */
    if (callback_arg != NULL) {
        /* 显式类型转换增强可读性 */
        ip_addr_t *result_buffer = callback_arg;
        *result_buffer = *ipaddr;
    } else {
        printf("Warning: Callback argument is NULL for %s\n", name);
    }
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief 通过域名获取IP地址
 * @param hostname 域名字符串
 * @return ip_addr_t 解析后的IP地址
 * @note 如果DNS查询失败，会触发系统复位
 */
ip_addr_t get_ip_by_hostname(const char *hostname) {
    ip_addr_t ip_addr= IPADDR4_INIT(IPADDR_ANY);   ;  // 初始化IP地址为ANY
    const err_t dns_err = dns_gethostbyname(hostname, &ip_addr, dns_query_callback, &ip_addr);  // 回调参数设为NULL

    // 检查DNS查询结果
    if (dns_err != ERR_OK && dns_err != ERR_INPROGRESS) {
        printf("DNS query for %s failed: %d\n", hostname, dns_err);
        NVIC_SystemReset();  // 查询失败，系统复位
    }

    // 等待DNS解析完成（IP地址不为空）
    while (ip_addr_isany_val(ip_addr)) {
        MX_LWIP_Process();  // 处理LWIP协议栈事件
        // 建议添加超时机制，避免无限等待
    }
    return ip_addr;
}

/**
 * @brief 获取NTP服务器IP地址
 * @return ip_addr_t NTP服务器IP地址
 */
ip_addr_t get_ntp_ip() {
    return get_ip_by_hostname("ntp.aliyun.com");
}

/**
 * @brief 获取API服务器IP地址
 * @return ip_addr_t API服务器IP地址
 */
ip_addr_t get_api_ip() {
    return get_ip_by_hostname("api.hemeng.org");
}