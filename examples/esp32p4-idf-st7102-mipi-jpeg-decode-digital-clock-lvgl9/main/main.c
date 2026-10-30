/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "lvgl_init.h"
#include "esp_sdmmc_card.h"
#include "esp_jpeg_decode.h"
#include <time.h> // 用于获取系统时间
#include <stdio.h> // 用于 snprintf

#define ROOT "/sdcard"
#define JPEG_FILENAME ROOT "/image_480_480_1.jpg"

#define FRAME_WIDTH 480  // 帧宽
#define FRAME_HEIGHT 480 // 帧高

#if BSP_LCD_COLOR_DEPTH == 16
#define OUTPUT_COLOR_BYTE 2 // 输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565
#elif BSP_LCD_COLOR_DEPTH == 24
#define OUTPUT_COLOR_BYTE 3 // 输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888
#endif

// 变量
static int16_t x = 0, y = 0, w = -1, h = -1;
static lv_obj_t *clock_label; // 数字时钟的标签

// 更新时钟显示的函数
static void update_clock(lv_timer_t *timer) {
    time_t now;
    struct tm *time_info;
    char time_str[9]; // 用于存储 HH:MM:SS 的缓冲区

    // 获取当前时间
    time(&now);
    time_info = localtime(&now);
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
             time_info->tm_hour, time_info->tm_min, time_info->tm_sec);

    // 更新标签文本（线程安全）
    lvgl_port_lock(0);
    lv_label_set_text(clock_label, time_str);
    lvgl_port_unlock();
}

void app_main(void) {
    // 初始化 LCD
    ESP_ERROR_CHECK(app_lcd_init());

    // 初始化 LVGL
    ESP_ERROR_CHECK(app_lvgl_init());

    // 初始化 SDMMC
    esp_sdmmc_pin_config_t pin_config = {
        .clk = SDMMC_CLK,    // 时钟引脚
        .cmd = SDMMC_CMD,    // 命令引脚
        .d0 = SDMMC_D0,      // 数据0引脚
        .d1 = SDMMC_D1,      // 数据1引脚
        .d2 = SDMMC_D2,      // 数据2引脚
        .d3 = SDMMC_D3,      // 数据3引脚
        .width = SDMMC_WIDTH, // 使用宏定义的总线宽度
        .slot = SDMMC_SLOT   // 使用宏定义的卡槽编号
    };
    esp_sdmmc_card_init(&pin_config);

    // 初始化 JPEG 解码器
    esp_jpeg_decode_t jpg = {
        .jpeg_buffer_size = FRAME_WIDTH * FRAME_HEIGHT, // 输入缓冲区大小
        .output_buffer_size = FRAME_WIDTH * FRAME_HEIGHT * OUTPUT_COLOR_BYTE, // 输出缓冲区大小
        .decode_cfg = {
            .output_format = JPEG_DECODE_OUT_FORMAT, // 输出格式
            .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR, // RGB 顺序
        },
    };
    esp_jpeg_decode_setup(&jpg, JPEG_FILENAME);

    // 解码 JPEG
    esp_jpeg_decode(&jpg);

    // 设置显示坐标
    w = esp_jpeg_decode_get_width(&jpg);
    h = esp_jpeg_decode_get_height(&jpg);

    // LVGL 线程安全锁
    lvgl_port_lock(0);

    // 创建 LVGL 图像对象
    lv_obj_t *img = lv_image_create(lv_screen_active());
    lv_image_dsc_t img_dsc = {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT,
            .w = w,
            .h = h,
            .stride = w * OUTPUT_COLOR_BYTE,
        },
        .data_size = w * h * OUTPUT_COLOR_BYTE,
        .data = esp_jpeg_decode_get_out_buf(&jpg),
    };

    // 设置图像源和位置
    lv_image_set_src(img, &img_dsc);
    lv_obj_set_pos(img, x, y);

    // 创建数字时钟标签
    clock_label = lv_label_create(lv_screen_active());
    lv_label_set_text(clock_label, "00:00:00"); // 初始文本
    lv_obj_align(clock_label, LV_ALIGN_TOP_RIGHT, -10, 10); // 定位在右上角

    // 为时钟标签设置透明样式
    static lv_style_t clock_style;
    lv_style_init(&clock_style);
    lv_style_set_text_font(&clock_style, &lv_font_montserrat_26); // 设置字体（可根据需要调整大小）
    lv_style_set_text_color(&clock_style, lv_color_white()); // 白色文本以提高可见性
    lv_style_set_bg_opa(&clock_style, LV_OPA_50); // 50% 透明背景
    lv_style_set_bg_color(&clock_style, lv_color_black()); // 黑色背景
    lv_style_set_border_width(&clock_style, 0); // 无边框
    lv_obj_add_style(clock_label, &clock_style, 0);

    // 创建定时器，每秒更新一次时钟
    lv_timer_create(update_clock, 1000, NULL);

    // 任务解锁
    lvgl_port_unlock();

    ESP_LOGI(TAG, "使用 LVGL 显示图像和数字时钟");

    // 清理
    esp_jpeg_decode_close(&jpg);
    esp_sdmmc_card_deinit();
    ESP_LOGI(TAG, "JPEG 处理结束");
}