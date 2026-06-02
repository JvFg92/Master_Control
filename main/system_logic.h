#ifndef SYSTEM_LOGIC_H
#define SYSTEM_LOGIC_H

#include <stdint.h>
#include <stddef.h>

// Definições globais compartilhadas
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE    1024

// Declaração das funções de controle
void serial_monitor_task(void *pvParameters);
void process_pc_command(const char* command, char* response_buffer, size_t max_resp_len);
void handle_nfc_detection(uint8_t *uid);

#endif // SYSTEM_LOGIC_H