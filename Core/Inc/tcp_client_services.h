//
// Created by hemeng on 25-4-19.
//

#ifndef TCP_CLIENT_SERVICES_H
#define TCP_CLIENT_SERVICES_H
#include <stdio.h>

#include "bme280.h"
#include "cJSON.h"
#include "ds3231.h"
#include "link.h"
#include "shtc.h"
#include "at24c32.h"
#include "crc16_modbus.h"
void heartbeat_handler();

void process_data();
#endif //TCP_CLIENT_SERVICES_H
