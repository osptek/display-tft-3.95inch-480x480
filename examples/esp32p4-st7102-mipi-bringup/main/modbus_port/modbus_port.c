#include "modbus_port.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbcontroller.h"

static const char *TAG = "modbus_port";

#define MB_PAR_INFO_TOUT_MS 10

#define MB_READ_MASK (MB_EVENT_INPUT_REG_RD | MB_EVENT_HOLDING_REG_RD)
#define MB_WRITE_MASK MB_EVENT_HOLDING_REG_WR
#define MB_READ_WRITE_MASK (MB_READ_MASK | MB_WRITE_MASK)

static void *s_slave_handle = NULL;
static uint16_t s_holding_regs[MODBUS_HOLDING_REG_COUNT];

static void modbus_event_task(void *arg)
{
    mb_param_info_t reg_info = {0};

    for (;;) {
        (void)mbc_slave_check_event(s_slave_handle, MB_READ_WRITE_MASK);
        esp_err_t err = mbc_slave_get_param_info(s_slave_handle, &reg_info, MB_PAR_INFO_TOUT_MS);
        if (err != ESP_OK) {
            continue;
        }

        if (reg_info.type & MB_EVENT_HOLDING_REG_WR) {
            ESP_LOGI(TAG, "HOLDING WRITE addr=%u size=%u",
                     (unsigned)reg_info.mb_offset, (unsigned)reg_info.size);
        }
    }
}

size_t modbus_port_get_holding_count(void)
{
    return MODBUS_HOLDING_REG_COUNT;
}

uint16_t modbus_port_get_holding(size_t index)
{
    if (index >= MODBUS_HOLDING_REG_COUNT || s_slave_handle == NULL) {
        return 0;
    }

    uint16_t value = 0;
    if (mbc_slave_lock(s_slave_handle) == ESP_OK) {
        value = s_holding_regs[index];
        (void)mbc_slave_unlock(s_slave_handle);
    }
    return value;
}

esp_err_t modbus_port_start(void)
{
    mb_communication_info_t comm = {
        .ser_opts.port = MODBUS_UART_PORT,
        .ser_opts.mode = MB_RTU,
        .ser_opts.baudrate = MODBUS_UART_BAUD_RATE,
        .ser_opts.parity = MB_PARITY_NONE,
        .ser_opts.uid = MODBUS_SLAVE_ADDR,
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1,
    };

    ESP_RETURN_ON_ERROR(mbc_slave_create_serial(&comm, &s_slave_handle), TAG, "create serial slave failed");

    for (size_t i = 0; i < MODBUS_HOLDING_REG_COUNT; ++i) {
        s_holding_regs[i] = (uint16_t)(i + 1);
    }

    mb_register_area_descriptor_t area = {
        .type = MB_PARAM_HOLDING,
        .start_offset = 0,
        .address = s_holding_regs,
        .size = sizeof(s_holding_regs),
        .access = MB_ACCESS_RW,
    };
    ESP_RETURN_ON_ERROR(mbc_slave_set_descriptor(s_slave_handle, area), TAG, "holding descriptor failed");

    /* SP3485: DE/RE on RTS for UART_MODE_RS485_HALF_DUPLEX */
    ESP_RETURN_ON_ERROR(uart_set_pin(MODBUS_UART_PORT,
                                     MODBUS_UART_TX_GPIO,
                                     MODBUS_UART_RX_GPIO,
                                     MODBUS_UART_DE_GPIO,
                                     UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin failed");
    ESP_RETURN_ON_ERROR(uart_set_mode(MODBUS_UART_PORT, UART_MODE_RS485_HALF_DUPLEX),
                        TAG, "uart_set_mode failed");

    ESP_RETURN_ON_ERROR(mbc_slave_start(s_slave_handle), TAG, "slave start failed");

    BaseType_t ok = xTaskCreate(modbus_event_task,
                                "modbus_evt",
                                MODBUS_EVENT_TASK_STACK,
                                NULL,
                                MODBUS_EVENT_TASK_PRIORITY,
                                NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create modbus event task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Modbus RTU slave addr=%d, %d 8N1, UART1 TX=%d RX=%d DE=%d, holding=%d",
             MODBUS_SLAVE_ADDR,
             MODBUS_UART_BAUD_RATE,
             MODBUS_UART_TX_GPIO,
             MODBUS_UART_RX_GPIO,
             MODBUS_UART_DE_GPIO,
             MODBUS_HOLDING_REG_COUNT);
    return ESP_OK;
}
