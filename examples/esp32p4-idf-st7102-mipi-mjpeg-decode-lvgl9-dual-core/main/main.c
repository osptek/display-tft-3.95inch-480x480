/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "lvgl_init.h"
#include "esp_sdmmc_card.h"
#include "esp_mjpeg_decode.h"

#define ROOT "/sdcard"
#define MJPEG_FILENAME ROOT "/mjpeg_480_480_30fps.mjpeg"

#define FRAME_WIDTH 480 // 帧宽
#define FRAME_HEIGHT 480 // 帧高

#if LV_COLOR_DEPTH ==16
#define OUTPUT_COLOR_BYTE 2 //输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565
#elif LV_COLOR_DEPTH ==24
#define OUTPUT_COLOR_BYTE 3 //输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888
#endif

// MJPEG 解码器实例
static esp_mjpeg_decode_t mjpeg = {
    .mjpeg_buffer_size = FRAME_WIDTH * FRAME_HEIGHT, // 输入缓冲区大小
    .output_buffer_size = FRAME_WIDTH * FRAME_HEIGHT * OUTPUT_COLOR_BYTE, // 输出缓冲区大小
    .decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT, // 输出格式
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR, // RGB 顺序
    }
};

// 变量
static lv_obj_t *video_img = NULL;        // LVGL 图像对象
static lv_image_dsc_t img_dsc;            // 图像描述符（用于原始缓冲区）
static bool decoding_active = true;       // 解码任务控制标志
static bool loop_playback = true;         // 控制是否循环播放

// MJPEG 解码任务（固定到核心 0）
static void mjpeg_decode_task(void *pvParameters)
{
    while (decoding_active) {
        while (decoding_active && esp_mjpeg_decode_read_mjpeg_buf(&mjpeg)) {
            // 解码帧到输出缓冲区
            esp_err_t ret = esp_mjpeg_decode_jpg(&mjpeg);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "MJPEG 帧解码失败: %s", esp_err_to_name(ret));
                continue; // 跳过失败的帧
            }

            // 更新 LVGL 图像（线程安全）
            lvgl_port_lock(0);
            img_dsc.header.w = esp_mjpeg_decode_get_width(&mjpeg);
            img_dsc.header.h = esp_mjpeg_decode_get_height(&mjpeg);
            img_dsc.data_size = img_dsc.header.w * img_dsc.header.h * (LV_COLOR_DEPTH / 8);
            img_dsc.data = (uint8_t *)mjpeg.output_buf; // 指向解码缓冲区
            lv_image_set_src(video_img, &img_dsc);
            lv_refr_now(lvgl_disp); // 立即刷新以播放视频
            lvgl_port_unlock();
        }

        // 如果不循环播放，退出循环
        if (!loop_playback) {
            decoding_active = false;
            break;
        }

        // 重置 MJPEG 文件读取位置
        ESP_LOGI(TAG, "Reached end of MJPEG file, restarting playback");
        if (esp_mjpeg_decode_reset(&mjpeg) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to reset MJPEG decoder");
            decoding_active = false;
            break;
        }
    }

    // 解码结束后清理
    ESP_LOGI(TAG, "MJPEG 解码任务完成");
    esp_mjpeg_decode_close(&mjpeg);
    if (video_img) {
        lvgl_port_lock(0);
        lv_obj_delete(video_img);
        video_img = NULL;
        lvgl_port_unlock();
    }
    esp_sdmmc_card_deinit();
    ESP_LOGI(TAG, "MJPEG 播放结束");
    vTaskDelete(NULL);
}

void app_main(void)
{
    // 初始化 LCD
    ESP_ERROR_CHECK(app_lcd_init());

    // 初始化 LVGL（固定到核心 1）
    ESP_ERROR_CHECK(app_lvgl_init());

    // 初始化 SDMMC
    esp_sdmmc_pin_config_t pin_config = {
        .clk = SDMMC_CLK,    // 时钟引脚
        .cmd = SDMMC_CMD,    // 命令引脚
        .d0 = SDMMC_D0,      // 数据 0 引脚
        .d1 = SDMMC_D1,      // 数据 1 引脚
        .d2 = SDMMC_D2,      // 数据 2 引脚
        .d3 = SDMMC_D3,      // 数据 3 引脚
        .width = SDMMC_WIDTH, // 总线宽度（通常为 4 位）
        .slot = SDMMC_SLOT   // SDMMC 卡槽编号
    };
    esp_err_t ret = esp_sdmmc_card_init(&pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC 初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化 MJPEG 解码器
    ret = esp_mjpeg_decode_setup(&mjpeg, MJPEG_FILENAME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MJPEG 解码器初始化失败: %s", esp_err_to_name(ret));
        esp_sdmmc_card_deinit();
        return;
    }

    // 创建 LVGL 屏幕和图像对象（仅创建一次）
    lvgl_port_lock(0);
    lv_obj_t *screen = lv_screen_active();
    video_img = lv_image_create(screen);
    lv_obj_center(video_img); // 图像居中
    lv_obj_set_size(video_img, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES); // 全屏显示
    memset(&img_dsc, 0, sizeof(img_dsc));
    img_dsc.header.cf = LV_COLOR_FORMAT; // 颜色格式
    lvgl_port_unlock();

    // 在核心 0 启动 MJPEG 解码任务
    xTaskCreatePinnedToCore(mjpeg_decode_task, "mjpeg_decode", 4096*2, NULL, 5, NULL, 0);

    // app_main 可处理其他任务或进入空闲状态
    ESP_LOGI(TAG, "MJPEG 解码任务已在核心 0 启动");
    ESP_LOGI(TAG, "循环播放: %s", loop_playback ? "启用" : "禁用");
}