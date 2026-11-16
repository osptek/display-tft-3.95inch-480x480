/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "hw_init.h"

#if CONFIG_EXAMPLE_LCD_INTERFACE_RGB

#include "esp_lcd_st7701.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"

static const char *TAG = "hw_lcd_init";

#define EXAMPLE_LCD_IO_SPI_CS (GPIO_NUM_45)
#define EXAMPLE_LCD_IO_SPI_SCL (GPIO_NUM_38)
#define EXAMPLE_LCD_IO_SPI_SDA (GPIO_NUM_39)

#define HW_LCD_PIXEL_CLOCK_HZ                   (18 * 1000 * 1000)
#define HW_LCD_HSYNC                            (10)
#define HW_LCD_HBP                              (10)
#define HW_LCD_HFP                              (20)
#define HW_LCD_VSYNC                            (10)
#define HW_LCD_VBP                              (10)
#define HW_LCD_VFP                              (10)

#define HW_LCD_RGB_VSYNC                        (GPIO_NUM_21)
#define HW_LCD_RGB_HSYNC                        (GPIO_NUM_14)
#define HW_LCD_RGB_DE                           (GPIO_NUM_47)
#define HW_LCD_RGB_PCLK                         (GPIO_NUM_48)
#define HW_LCD_RGB_DISP                         (GPIO_NUM_NC)
#define HW_LCD_RGB_DATA0                        (GPIO_NUM_0)
#define HW_LCD_RGB_DATA1                        (GPIO_NUM_12)
#define HW_LCD_RGB_DATA2                        (GPIO_NUM_11)
#define HW_LCD_RGB_DATA3                        (GPIO_NUM_10)
#define HW_LCD_RGB_DATA4                        (GPIO_NUM_9)
#define HW_LCD_RGB_DATA5                        (GPIO_NUM_46)
#define HW_LCD_RGB_DATA6                        (GPIO_NUM_3)
#define HW_LCD_RGB_DATA7                        (GPIO_NUM_20)
#define HW_LCD_RGB_DATA8                        (GPIO_NUM_19)
#define HW_LCD_RGB_DATA9                        (GPIO_NUM_8)
#define HW_LCD_RGB_DATA10                       (GPIO_NUM_18)
#define HW_LCD_RGB_DATA11                       (GPIO_NUM_17)
#define HW_LCD_RGB_DATA12                       (GPIO_NUM_16)
#define HW_LCD_RGB_DATA13                       (GPIO_NUM_15)
#define HW_LCD_RGB_DATA14                       (GPIO_NUM_7)
#define HW_LCD_RGB_DATA15                       (GPIO_NUM_6)

#define EXAMPLE_LCD_IO_RST (-1)                // -1 if not used
#define EXAMPLE_PIN_NUM_BK_LIGHT (GPIO_NUM_13) // -1 if not used
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL (1)
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

#define EXAMPLE_LCD_BIT_PER_PIXEL (18)
#define HW_LCD_DATA_WIDTH                       (16)
#define HW_LCD_BIT_PER_PIXEL                    (16)
#define HW_LCD_BOUNCE_BUFFER_HEIGHT             (20)

static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0B, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x37, 0x02}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x09, 0x08, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18}, 16, 0},
    {0xB1, (uint8_t[]){0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x08, 0x09, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x4D}, 1, 0},
    {0xB1, (uint8_t[]){0x33}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB5, (uint8_t[]){0x4B}, 1, 0},
    {0xB7, (uint8_t[]){0x8C}, 1, 0},
    {0xB8, (uint8_t[]){0x20}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x02, 0xF0, 0x00, 0x00, 0x03, 0xF0, 0x00, 0x00, 0x00, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t[]){0x10, 0x10, 0x40, 0x40, 0xF2, 0xF0, 0x00, 0x00, 0xF2, 0xF0, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x07, 0xEF, 0xF0, 0xF0, 0x09, 0xF1, 0xF0, 0xF0, 0x03, 0xF3, 0xF0, 0xF0, 0x05, 0xED, 0xF0, 0xF0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0xF0, 0xF0, 0xF0, 0x0A, 0xF2, 0xF0, 0xF0, 0x04, 0xF4, 0xF0, 0xF0, 0x06, 0xEE, 0xF0, 0xF0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0xE4, 0xE4, 0x44, 0x88, 0x40}, 7, 0},
    {0xEC, (uint8_t[]){0x78, 0x00}, 2, 0},
    {0xED, (uint8_t[]){0x20, 0xF9, 0x87, 0x76, 0x65, 0x54, 0x4F, 0xFF, 0xFF, 0xF4, 0x45, 0x56, 0x67, 0x78, 0x9F, 0x02}, 16, 0},
    {0xEF, (uint8_t[]){0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}, 6, 0},

    // 退出睡眠模式和开启显示的命令保持不变
    {0x11, (uint8_t[]){0x00}, 0, 120}, // Sleep Out
    {0x29, (uint8_t[]){0x00}, 0, 0},   // Display On
};

static esp_lcd_panel_handle_t s_panel_handle;

esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *panel_handle, esp_lcd_panel_io_handle_t *io_handle, esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode, esp_lv_adapter_rotation_t rotation)
{

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    {
        ESP_LOGI(TAG, "Turn off LCD backlight");
        gpio_config_t bk_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT};
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    }
#endif

    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = EXAMPLE_LCD_IO_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = EXAMPLE_LCD_IO_SPI_SCL,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = EXAMPLE_LCD_IO_SPI_SDA,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, io_handle));

    ESP_LOGI(TAG, "Initialize RGB panel");
    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dma_burst_size = 64,
        .data_width = HW_LCD_DATA_WIDTH,
        .bits_per_pixel = HW_LCD_BIT_PER_PIXEL,
        .de_gpio_num = HW_LCD_RGB_DE,
        .pclk_gpio_num = HW_LCD_RGB_PCLK,
        .vsync_gpio_num = HW_LCD_RGB_VSYNC,
        .hsync_gpio_num = HW_LCD_RGB_HSYNC,
        .disp_gpio_num = HW_LCD_RGB_DISP,
        .data_gpio_nums = {
            HW_LCD_RGB_DATA0,
            HW_LCD_RGB_DATA1,
            HW_LCD_RGB_DATA2,
            HW_LCD_RGB_DATA3,
            HW_LCD_RGB_DATA4,
            HW_LCD_RGB_DATA5,
            HW_LCD_RGB_DATA6,
            HW_LCD_RGB_DATA7,
            HW_LCD_RGB_DATA8,
            HW_LCD_RGB_DATA9,
            HW_LCD_RGB_DATA10,
            HW_LCD_RGB_DATA11,
            HW_LCD_RGB_DATA12,
            HW_LCD_RGB_DATA13,
            HW_LCD_RGB_DATA14,
            HW_LCD_RGB_DATA15,
        },
        .timings = {
            .pclk_hz = HW_LCD_PIXEL_CLOCK_HZ,
            .h_res = HW_LCD_H_RES,
            .v_res = HW_LCD_V_RES,
            .hsync_back_porch = HW_LCD_HBP,
            .hsync_front_porch = HW_LCD_HFP,
            .hsync_pulse_width = HW_LCD_HSYNC,
            .vsync_back_porch = HW_LCD_VBP,
            .vsync_front_porch = HW_LCD_VFP,
            .vsync_pulse_width = HW_LCD_VSYNC,
            .flags = {
                .pclk_active_neg = false,
            },
        },
        .flags.fb_in_psram = 1,
        .num_fbs = esp_lv_adapter_get_required_frame_buffer_count(tear_avoid_mode, rotation),
        .bounce_buffer_size_px = HW_LCD_H_RES * HW_LCD_BOUNCE_BUFFER_HEIGHT,
    };
    st7701_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .init_cmds = lcd_init_cmds, // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .mirror_by_cmd = 1,     // Only work when `enable_io_multiplex` is set to 0
            .enable_io_multiplex = 0, /**
                                     * Set to 1 if panel IO is no longer needed after LCD initialization.
                                     * If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                     * Please set it to 1 to release the pins.
                                     */
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_LCD_IO_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(*io_handle, &panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
        ESP_LOGI(TAG, "Turn on LCD backlight");
        gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    *panel_handle = s_panel_handle;
    if (io_handle) {
        *io_handle = NULL;
    }

    return ESP_OK;
}

esp_err_t hw_lcd_deinit(void)
{
    if (s_panel_handle) {
        esp_lcd_panel_disp_on_off(s_panel_handle, false);
        esp_lcd_panel_del(s_panel_handle);
        s_panel_handle = NULL;
    }

    return ESP_OK;
}

int hw_lcd_get_te_gpio(void)
{
    return GPIO_NUM_NC;
}

uint32_t hw_lcd_get_bus_freq_hz(void)
{
    return 0;
}

uint8_t hw_lcd_get_bus_data_lines(void)
{
    return HW_LCD_DATA_WIDTH;
}

uint8_t hw_lcd_get_bits_per_pixel(void)
{
    return HW_LCD_BIT_PER_PIXEL;
}

#endif
