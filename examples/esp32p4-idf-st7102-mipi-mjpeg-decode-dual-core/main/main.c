/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "lcd_init.h"
#include "esp_sdmmc_card.h"
#include "esp_mjpeg_decode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define ROOT "/sdcard"
#define MJPEG_FILENAME ROOT "/mjpeg_480_480_30fps.mjpeg"

#define FRAME_WIDTH 480 // 帧宽
#define FRAME_HEIGHT 480 // 帧高

#if BSP_LCD_COLOR_DEPTH ==16
#define OUTPUT_COLOR_BYTE 2 //输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565
#elif BSP_LCD_COLOR_DEPTH ==24
#define OUTPUT_COLOR_BYTE 3 //输出颜色字节
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888
#endif

// 变量
static int total_frames = 0; // 总帧数
static uint64_t total_read_video = 0; // 读取视频总时间
static uint64_t total_decode_video = 0; // 解码视频总时间
static uint64_t total_show_video = 0; // 显示视频总时间
static uint64_t start_ms; // 开始时间
static int16_t x = -1, y = -1, w = -1, h = -1; // 显示坐标和尺寸
static int lcd_width = 480, lcd_height = 480;  // LCD 尺寸
static bool loop_playback = true; // 控制是否循环播放

// MJPEG 解码器实例
static esp_mjpeg_decode_t mjpeg = {
    .mjpeg_buffer_size = FRAME_WIDTH * FRAME_HEIGHT, // 输入缓冲区大小
    .output_buffer_size = FRAME_WIDTH * FRAME_HEIGHT * OUTPUT_COLOR_BYTE, // 输出缓冲区大小
    .decode_cfg = {
        .output_format = JPEG_DECODE_OUT_FORMAT, // 输出格式
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR, // RGB 顺序
    }
};

// 队列用于传递解码后的帧
static QueueHandle_t frame_queue;

// 帧数据结构
typedef struct {
    uint8_t *buffer; // 解码后的帧数据
    size_t size;      // 缓冲区大小
} frame_data_t;

// 任务1：读取和解码（运行在核心0）
void read_decode_task(void *pvParameters) {
    uint64_t curr_ms, prev;

    // 初始化 MJPEG 解码器
    if (esp_mjpeg_decode_setup(&mjpeg, MJPEG_FILENAME) != ESP_OK) {
        ESP_LOGE(TAG, "MJPEG 解码器初始化失败");
        // 发送终止信号
        frame_data_t end_frame = { .buffer = NULL, .size = 0 };
        xQueueSend(frame_queue, &end_frame, portMAX_DELAY);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        while (esp_mjpeg_decode_read_mjpeg_buf(&mjpeg)) {
            curr_ms = esp_timer_get_time() / 1000;
            total_read_video += curr_ms - start_ms;
            prev = curr_ms;

            // 解码
            if (esp_mjpeg_decode_jpg(&mjpeg) != ESP_OK) continue;
            curr_ms = esp_timer_get_time() / 1000;
            total_decode_video += curr_ms - prev;

            // 获取解码后的帧数据
            frame_data_t frame = {
                .buffer = esp_mjpeg_decode_get_out_buf(&mjpeg),
                .size = mjpeg.output_buffer_size
            };

            // 发送到显示任务
            if (xQueueSend(frame_queue, &frame, portMAX_DELAY) != pdTRUE) {
                ESP_LOGE(TAG, "无法将帧发送到队列");
            }

            total_frames++;
        }

        // 如果不循环播放，退出循环
        if (!loop_playback) {
            break;
        }

        // 重置 MJPEG 文件读取位置
        ESP_LOGI(TAG, "Reached end of MJPEG file, restarting playback");
        if (esp_mjpeg_decode_reset(&mjpeg) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to reset MJPEG decoder");
            break;
        }

        // 重置帧计数和时间统计以避免累积
        total_frames = 0;
        total_read_video = 0;
        total_decode_video = 0;
        total_show_video = 0;
        start_ms = esp_timer_get_time() / 1000;
    }

    // 发送终止信号
    frame_data_t end_frame = { .buffer = NULL, .size = 0 };
    xQueueSend(frame_queue, &end_frame, portMAX_DELAY);

    // 清理
    esp_mjpeg_decode_close(&mjpeg);
    vTaskDelete(NULL);
}

// 任务2：显示帧（运行在核心1）
void display_task(void *pvParameters) {
    frame_data_t frame;
    uint64_t curr_ms, prev;

    while (1) {
        // 从队列接收帧
        if (xQueueReceive(frame_queue, &frame, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // 检查终止信号
        if (frame.buffer == NULL) {
            break;
        }

        prev = esp_timer_get_time() / 1000;

        // 设置显示坐标
        if (x == -1) {
            w = esp_mjpeg_decode_get_width(&mjpeg);
            h = esp_mjpeg_decode_get_height(&mjpeg);
            x = (w > lcd_width) ? 0 : ((lcd_width - w) / 2);
            y = (h > lcd_height) ? 0 : ((lcd_height - h) / 2);
        }

        // 显示帧
        drawRGBBitmap(x, y, w, h, frame.buffer);
        curr_ms = esp_timer_get_time() / 1000;
        total_show_video += curr_ms - prev;
    }

    vTaskDelete(NULL);
}

void app_main(void) {
    start_ms = esp_timer_get_time() / 1000;

    // 初始化 LCD
    ESP_ERROR_CHECK(app_lcd_init());

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

    // 创建帧队列
    frame_queue = xQueueCreate(2, sizeof(frame_data_t));
    if (frame_queue == NULL) {
        ESP_LOGE(TAG, "无法创建帧队列");
        esp_sdmmc_card_deinit();
        return;
    }

    // 创建任务并绑定到不同核心
    xTaskCreatePinnedToCore(read_decode_task, "read_decode_task", 8192, NULL, 5, NULL, 0); // 核心0
    xTaskCreatePinnedToCore(display_task, "display_task", 4096, NULL, 5, NULL, 1);         // 核心1

    // 主任务等待任务完成（实际上由任务自行清理）
    vTaskDelay(portMAX_DELAY);

    // 性能报告
    uint64_t time_used = esp_timer_get_time() / 1000 - start_ms;
    float fps = 1000.0f * total_frames / time_used;
    ESP_LOGI(TAG, "ESP32-P4 MJPEG 解码器");
    ESP_LOGI(TAG, "帧尺寸: %d x %d", esp_mjpeg_decode_get_width(&mjpeg), esp_mjpeg_decode_get_height(&mjpeg));
    ESP_LOGI(TAG, "总帧数: %d", total_frames);
    ESP_LOGI(TAG, "使用时间: %llu 毫秒", time_used);
    ESP_LOGI(TAG, "平均帧率: %.1f", fps);
    ESP_LOGI(TAG, "读取 MJPEG: %llu 毫秒 (%.1f %%)", total_read_video, 100.0f * total_read_video / time_used);
    ESP_LOGI(TAG, "解码视频: %llu 毫秒 (%.1f %%)", total_decode_video, 100.0f * total_decode_video / time_used);
    ESP_LOGI(TAG, "显示视频: %llu 毫秒 (%.1f %%)", total_show_video, 100.0f * total_show_video / time_used);

    // 清理
    esp_sdmmc_card_deinit();
    vQueueDelete(frame_queue);
    ESP_LOGI(TAG, "MJPEG 结束");
}