/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "lcd_init.h"
#include "esp_sdmmc_card.h"
#include "esp_jpeg_decode.h"
#include <dirent.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define ROOT "/sdcard/jpeg/jpeg_480_480" // SD卡子目录
#define FRAME_WIDTH 480  // 帧宽度
#define FRAME_HEIGHT 480 // 帧高度
#define DISPLAY_DELAY_MS 2000 // 每张图片显示的延迟（毫秒）

#if BSP_LCD_COLOR_DEPTH == 16
#define OUTPUT_COLOR_BYTE 2 // 输出颜色字节数
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565 // 16位颜色格式
#elif BSP_LCD_COLOR_DEPTH == 24
#define OUTPUT_COLOR_BYTE 3 // 输出颜色字节数
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888 // 24位颜色格式
#endif

// 变量
static int16_t x = 0, y = 0, w = -1, h = -1; // 显示坐标和图片宽高

// 显示单张JPEG图片的函数
void display_jpeg(const char *filename) {
    // 初始化JPEG解码器
    esp_jpeg_decode_t jpg = {
        .jpeg_buffer_size = FRAME_WIDTH * FRAME_HEIGHT, // 输入缓冲区大小
        .output_buffer_size = FRAME_WIDTH * FRAME_HEIGHT * OUTPUT_COLOR_BYTE, // 输出缓冲区大小
        .decode_cfg = {
            .output_format = JPEG_DECODE_OUT_FORMAT, // 输出格式
            .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR, // RGB颜色顺序
        },
    };

    // 设置并解码JPEG
    ESP_ERROR_CHECK(esp_jpeg_decode_setup(&jpg, filename)); // 配置解码器
    ESP_ERROR_CHECK(esp_jpeg_decode(&jpg)); // 执行解码

    // 设置显示坐标
    w = esp_jpeg_decode_get_width(&jpg); // 获取图片宽度
    h = esp_jpeg_decode_get_height(&jpg); // 获取图片高度

    // 显示图片
    uint8_t *out_buf = esp_jpeg_decode_get_out_buf(&jpg); // 获取解码后的缓冲区
    drawRGBBitmap(x, y, w, h, out_buf); // 在LCD上绘制图片
    ESP_LOGI(TAG, "显示图片: %s", filename); // 记录显示的图片

    // 清理JPEG解码器
    esp_jpeg_decode_close(&jpg);
}

// 主函数
void app_main(void) {
    // 初始化LCD
    ESP_ERROR_CHECK(app_lcd_init());

    // 初始化SDMMC
    esp_sdmmc_pin_config_t pin_config = {
        .clk = SDMMC_CLK, // 时钟引脚
        .cmd = SDMMC_CMD, // 命令引脚
        .d0 = SDMMC_D0,   // 数据0引脚
        .d1 = SDMMC_D1,   // 数据1引脚
        .d2 = SDMMC_D2,   // 数据2引脚
        .d3 = SDMMC_D3,   // 数据3引脚
        .width = SDMMC_WIDTH, // 总线宽度
        .slot = SDMMC_SLOT,   // 卡槽编号
    };
    ESP_ERROR_CHECK(esp_sdmmc_card_init(&pin_config)); // 初始化SD卡

    // 打开目录
    DIR *dir = opendir(ROOT);
    if (!dir) {
        ESP_LOGE(TAG, "无法打开目录: %s", ROOT); // 目录打开失败
        esp_sdmmc_card_deinit(); // 清理SD卡
        return;
    }

    // 循环显示图片
    while (1) {
        struct dirent *entry;
        char filepath[256];
        int image_count = 0; // 统计当前循环显示的图片数量
        while ((entry = readdir(dir)) != NULL) {
            // 检查文件是否为.jpg或.jpeg
            if (strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".jpeg")) {
                // 检查文件名长度
                size_t root_len = strlen(ROOT);
                size_t name_len = strlen(entry->d_name);
                if (root_len + name_len + 2 > sizeof(filepath)) { // +2 for '/' and '\0'
                    ESP_LOGE(TAG, "文件路径过长: %s", entry->d_name);
                    continue; // 跳过过长的文件名
                }
                snprintf(filepath, sizeof(filepath), "%s/%s", ROOT, entry->d_name); // 构造文件路径
                ESP_LOGI(TAG, "处理文件: %s", filepath); // 记录正在处理的文件

                // 显示JPEG文件
                display_jpeg(filepath);
                image_count++;

                // 延迟以显示图片一段时间
                vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY_MS));
            }
        }

        // 检查是否找到图片
        if (image_count == 0) {
            ESP_LOGW(TAG, "目录 %s 中未找到JPEG图片", ROOT);
            break; // 如果没有图片，退出循环
        }

        // 重置目录指针以重新开始
        rewinddir(dir);
        ESP_LOGI(TAG, "完成一轮图片显示，重新开始");
    }

    // 关闭目录并清理
    closedir(dir); // 关闭目录
    esp_sdmmc_card_deinit(); // 清理SD卡
    ESP_LOGI(TAG, "程序退出");
}
