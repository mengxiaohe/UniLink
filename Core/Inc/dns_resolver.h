//
// Created by hemeng on 2025/4/1.
//

#ifndef DNS_RESOLVER_H
#define DNS_RESOLVER_H

#include "ip_addr.h"

#include "dns.h"
#include "lwip.h"

ip_addr_t get_ntp_ip();

ip_addr_t get_api_ip();
#endif //DNS_RESOLVER_H
