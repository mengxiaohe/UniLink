//
// Created by hemeng on 25-4-22.
//

#ifndef MEMORY_SECTIONS_H
#define MEMORY_SECTIONS_H
#include <stdint.h>

#define LAN_PACKET_RX_BUFFER_SIZE (1024 * 1024 * 4)
#define LAN_PACKET_TX_BUFFER_SIZE (1024 * 1024 * 4)
#define PACKET_BUFFER_SIZE (1024 * 1024 * 4)

uint8_t packet_rx_buffer[LAN_PACKET_RX_BUFFER_SIZE] __attribute__((section(".packet_rx_buffer"))) = {0};
uint8_t packet_tx_buffer[LAN_PACKET_TX_BUFFER_SIZE] __attribute__((section(".packet_tx_buffer"))) = {0};

uint8_t packet_buffer[PACKET_BUFFER_SIZE] __attribute__((section(".packet_buffer"))) = {0};

#endif //MEMORY_SECTIONS_H
