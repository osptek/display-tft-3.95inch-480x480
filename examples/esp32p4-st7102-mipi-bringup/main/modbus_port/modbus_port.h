#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Modbus RTU slave over RS485 (SP3485)
 *   UART1 TX -> GPIO53 (DI)
 *   UART1 RX -> GPIO51 (RO)
 *   UART1 RTS -> GPIO52 (DE/RE)
 */
#define MODBUS_SLAVE_ADDR           1
#define MODBUS_UART_BAUD_RATE       115200
#define MODBUS_UART_PORT            UART_NUM_1
#define MODBUS_UART_TX_GPIO         GPIO_NUM_53
#define MODBUS_UART_RX_GPIO         GPIO_NUM_51
#define MODBUS_UART_DE_GPIO         GPIO_NUM_52
#define MODBUS_HOLDING_REG_COUNT    10
#define MODBUS_EVENT_TASK_PRIORITY  3
#define MODBUS_EVENT_TASK_STACK     4096

esp_err_t modbus_port_start(void);

size_t modbus_port_get_holding_count(void);
uint16_t modbus_port_get_holding(size_t index);

#ifdef __cplusplus
}
#endif
