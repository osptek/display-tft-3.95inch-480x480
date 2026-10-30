/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "hw_init.h"
#include "esp_lv_adapter.h"
#include "lv_demos.h"

static const char *TAG = "main";

/**
 * @brief 从Kconfig中获取配置的显示屏旋转角度
 *
 * @return esp_lv_adapter_rotation_t 旋转角度值
 */
static esp_lv_adapter_rotation_t get_configured_rotation(void)
{
#if CONFIG_EXAMPLE_DISPLAY_ROTATION_0
    return ESP_LV_ADAPTER_ROTATE_0;          // 旋转0度
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_90
    return ESP_LV_ADAPTER_ROTATE_90;         // 旋转90度
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_180
    return ESP_LV_ADAPTER_ROTATE_180;        // 旋转180度
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_270
    return ESP_LV_ADAPTER_ROTATE_270;        // 旋转270度
#else
    return ESP_LV_ADAPTER_ROTATE_0;          // 默认旋转0度
#endif
}

void app_main()
{
    esp_lcd_panel_handle_t display_panel = NULL;       // LCD面板句柄
    esp_lcd_panel_io_handle_t display_io_handle = NULL;// LCD IO接口句柄
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();  // 获取配置的旋转角度

    /* 根据LCD接口类型选择消抖模式 */
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI;
    ESP_LOGI(TAG, "已选择LCD接口: MIPI DSI");

    /* 初始化LCD硬件面板 */
    ESP_LOGI(TAG, "初始化LCD: 分辨率%dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_ERROR_CHECK(hw_lcd_init(&display_panel, &display_io_handle, tear_avoid_mode, rotation));

    /* 初始化LVGL适配器 */
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    /* 使用相应配置将显示屏注册到LVGL适配器 */
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);

    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (disp == NULL) {
        ESP_LOGE(TAG, "显示屏注册失败");
        return;
    }

    /* 根据接口类型初始化输入设备 */
    ESP_LOGI(TAG, "初始化触摸面板");
    esp_lcd_touch_handle_t touch_handle = NULL;    // 触摸面板句柄
    ESP_ERROR_CHECK(hw_touch_init(&touch_handle, rotation));

    /* 使用默认配置宏进行快速设置，采用1:1坐标缩放 */
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
    if (touch == NULL) {
        ESP_LOGE(TAG, "触摸设备注册失败");
        return;
    }

    /* 启动LVGL适配器 */
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    ESP_LOGI(TAG, "启动LVGL控件演示程序");
    /* 加锁互斥量，因为LVGL API不是线程安全的 */
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_demo_widgets();          // 运行LVGL控件演示
        // lv_demo_benchmark();      // 运行LVGL性能基准测试（注释掉）
        esp_lv_adapter_unlock();    // 解锁互斥量
    }
}
