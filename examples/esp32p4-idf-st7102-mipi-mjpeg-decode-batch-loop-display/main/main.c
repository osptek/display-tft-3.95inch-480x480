/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "lcd_init.h"
#include "esp_sdmmc_card.h"
#include "esp_mjpeg_decode.h"
#include "esp_log.h"
#include <dirent.h>
#include <string.h>

#define ROOT "/sdcard/mjpeg/mjpeg_480_480" // SD卡子目录
#define FRAME_WIDTH 480      // 帧宽
#define FRAME_HEIGHT 480     // 帧高

#if BSP_LCD_COLOR_DEPTH == 16
#define OUTPUT_COLOR_BYTE 2 // 输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565
#elif BSP_LCD_COLOR_DEPTH == 24
#define OUTPUT_COLOR_BYTE 3 // 输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888
#endif

// 变量
static int16_t x = -1, y = -1, w = -1, h = -1;
static int lcd_width = 480, lcd_height = 480; // 假设 LCD 尺寸

// MJPEG 解码器实例
static esp_mjpeg_decode_t mjpeg = {
    .mjpeg_buffer_size = FRAME_WIDTH * FRAME_HEIGHT, // 输入缓冲区大小
    .output_buffer_size = FRAME_WIDTH * FRAME_HEIGHT * OUTPUT_COLOR_BYTE, // 输出缓冲区大小
    .decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT, // 输出格式
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR, // RGB 顺序
    }
};

// 播放 MJPEG 的函数
static void play_mjpeg(const char *mjpeg_filename, bool loop_playback) {
    // 初始化 MJPEG 解码器
    if (esp_mjpeg_decode_setup(&mjpeg, mjpeg_filename) != ESP_OK) {
        ESP_LOGE(TAG, "esp_mjpeg_decode_setup failed for file: %s", mjpeg_filename);
        return;
    }

    while (true) {
        while (esp_mjpeg_decode_read_mjpeg_buf(&mjpeg)) {
            // 解码
            if (esp_mjpeg_decode_jpg(&mjpeg) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to decode JPEG frame, skipping");
                continue;
            }

            // 设置显示坐标
            if (x == -1) {
                w = esp_mjpeg_decode_get_width(&mjpeg);
                h = esp_mjpeg_decode_get_height(&mjpeg);
                x = (w > lcd_width) ? 0 : ((lcd_width - w) / 2);
                y = (h > lcd_height) ? 0 : ((lcd_height - h) / 2);
            }

            // 显示
            drawRGBBitmap(x, y, w, h, esp_mjpeg_decode_get_out_buf(&mjpeg));
        }

        // 如果不循环播放单个文件，退出循环
        if (!loop_playback) {
            break;
        }

        // 重置 MJPEG 文件读取位置
        ESP_LOGI(TAG, "Reached end of MJPEG file %s, restarting playback", mjpeg_filename);
        if (esp_mjpeg_decode_reset(&mjpeg) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to reset MJPEG decoder");
            break;
        }
    }

    // 清理解码器
    esp_mjpeg_decode_close(&mjpeg);
}

void app_main(void) {
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
    if (esp_sdmmc_card_init(&pin_config) != ESP_OK) {
        ESP_LOGE(TAG, "SDMMC initialization failed");
        return;
    }

    // 初始化 LCD
    if (app_lcd_init() != ESP_OK) {
        ESP_LOGE(TAG, "LCD initialization failed");
        return;
    }

    // 打开目录
    DIR *dir = opendir(ROOT);
    if (!dir) {
        ESP_LOGE(TAG, "无法打开目录: %s", ROOT);
        esp_sdmmc_card_deinit();
        return;
    }

    // 循环显示 MJPEG 文件
    bool loop_batch = true; // 是否循环播放整个目录
    while (true) {
        struct dirent *entry;
        char filepath[256];
        int file_count = 0; // 统计当前循环的文件数量
        while ((entry = readdir(dir)) != NULL) {
            // 检查文件是否为 .mjpeg
            if (strstr(entry->d_name, ".mjpeg")) {
                // 检查文件名长度
                size_t root_len = strlen(ROOT);
                size_t name_len = strlen(entry->d_name);
                if (root_len + name_len + 2 > sizeof(filepath)) { // +2 for '/' and '\0'
                    ESP_LOGE(TAG, "文件路径过长: %s", entry->d_name);
                    continue;
                }
                snprintf(filepath, sizeof(filepath), "%s/%s", ROOT, entry->d_name);
                ESP_LOGI(TAG, "处理文件: %s", filepath);

                // 播放 MJPEG 文件
                bool loop_playback = false; // 单个文件不循环播放
                play_mjpeg(filepath, loop_playback);

                // 重置显示坐标
                x = -1;
                y = -1;
            }
        }

        // 检查是否找到 MJPEG 文件
        if (file_count == 0) {
            ESP_LOGW(TAG, "目录 %s 中未找到 MJPEG 文件", ROOT);
            break;
        }

        // 如果不循环播放整个目录，退出
        if (!loop_batch) {
            break;
        }

        // 重置目录指针以重新开始
        rewinddir(dir);
        ESP_LOGI(TAG, "完成一轮 MJPEG 文件播放，重新开始");
    }

    // 关闭目录
    closedir(dir);

    // 清理 SDMMC
    esp_sdmmc_card_deinit();
    ESP_LOGI(TAG, "程序退出");
}
