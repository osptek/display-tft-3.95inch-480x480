/*
 * SPDX-FileCopyrightText: Copyright 2026 OSPTEK
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * https://github.com/osptek
 */

#include "lvgl_init.h"
#include "esp_sdmmc_card.h"
#include "esp_jpeg_decode.h"
#include "dirent.h" // 用于目录操作
#include <string.h>
#include <unistd.h> // 用于 usleep

#define ROOT "/sdcard/jpeg/jpeg_480_480" // SD卡子目录
#define FRAME_WIDTH 480  // 帧宽度
#define FRAME_HEIGHT 480 // 帧高度
#define DISPLAY_DELAY_MS 2000 // 每张图片显示的延迟时间（毫秒）

#if BSP_LCD_COLOR_DEPTH == 16
#define OUTPUT_COLOR_BYTE 2 // 输出颜色字节数
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB565
#elif BSP_LCD_COLOR_DEPTH == 24
#define OUTPUT_COLOR_BYTE 3 // 输出颜色字节数
#define JPEG_DECODE_OUT_FORMAT JPEG_DECODE_OUT_FORMAT_RGB888
#endif

// 变量
static int16_t x = 0, y = 0, w = -1, h = -1; // 图像坐标和尺寸
static lv_obj_t *img = NULL; // 全局图像对象，复用以避免内存泄漏

// 函数：获取目录中的 JPEG 文件列表
static void get_jpeg_files(const char *path, char *file_list[], int *file_count, int max_files) {
    DIR *dir;
    struct dirent *entry;
    *file_count = 0;

    dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "无法打开目录: %s", path); // 错误日志：无法打开目录
        return;
    }

    while ((entry = readdir(dir)) != NULL && *file_count < max_files) {
        // 检查文件扩展名是否为 .jpg 或 .jpeg
        if (strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".jpeg")) {
            file_list[*file_count] = malloc(strlen(path) + strlen(entry->d_name) + 2);
            sprintf(file_list[*file_count], "%s/%s", path, entry->d_name);
            (*file_count)++;
        }
    }
    closedir(dir);
}

// 函数：显示单张 JPEG 图片
static void display_jpeg(const char *filename, lv_obj_t *scr) {
    // 初始化 JPEG 解码器
    esp_jpeg_decode_t jpg = {
        .jpeg_buffer_size = FRAME_WIDTH * FRAME_HEIGHT, // 输入缓冲区大小
        .output_buffer_size = FRAME_WIDTH * FRAME_HEIGHT * OUTPUT_COLOR_BYTE, // 输出缓冲区大小
        .decode_cfg = {
            .output_format = JPEG_DECODE_OUT_FORMAT, // 输出格式
            .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR, // RGB 顺序
        },
    };
    esp_jpeg_decode_setup(&jpg, filename); // 设置 JPEG 解码器

    // 解码 JPEG
    esp_jpeg_decode(&jpg);

    // 获取图像尺寸
    w = esp_jpeg_decode_get_width(&jpg);
    h = esp_jpeg_decode_get_height(&jpg);

    // LVGL 线程安全锁
    lvgl_port_lock(0);

    // 如果图像对象不存在，创建新的
    if (!img) {
        img = lv_image_create(scr);
    }

    // 设置图像描述符
    lv_image_dsc_t img_dsc = {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC, // 图像头魔术字
            .cf = LV_COLOR_FORMAT, // 颜色格式
            .w = w, // 宽度
            .h = h, // 高度
            .stride = w * OUTPUT_COLOR_BYTE, // 行跨距
        },
        .data_size = w * h * OUTPUT_COLOR_BYTE, // 数据大小
        .data = esp_jpeg_decode_get_out_buf(&jpg), // 输出缓冲区
    };

    // 设置图像源和位置
    lv_image_set_src(img, &img_dsc);
    lv_obj_set_pos(img, x, y);

    // 解锁 LVGL
    lvgl_port_unlock();

    ESP_LOGI(TAG, "显示图像: %s", filename); // 信息日志：显示图像

    // 清理 JPEG 解码器
    esp_jpeg_decode_close(&jpg);
}

void app_main(void) {
    // 初始化 LCD
    ESP_ERROR_CHECK(app_lcd_init());

    // 初始化 LVGL
    ESP_ERROR_CHECK(app_lvgl_init());

    // 初始化 SDMMC
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
    esp_sdmmc_card_init(&pin_config); // 初始化 SD 卡

    // 获取当前屏幕
    lv_obj_t *scr = lv_scr_act();

    // 获取 JPEG 文件列表
    char *file_list[100]; // 假设最多 100 个文件
    int file_count = 0;
    get_jpeg_files(ROOT, file_list, &file_count, 100);

    if (file_count == 0) {
        ESP_LOGE(TAG, "未找到 JPEG 文件"); // 错误日志：未找到 JPEG 文件
        esp_sdmmc_card_deinit();
        return;
    }

    // 循环显示图片
    while (1) {
        for (int i = 0; i < file_count; i++) {
            display_jpeg(file_list[i], scr);
            vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY_MS)); // 延迟 2 秒
        }
    }

    // 清理文件列表
    for (int i = 0; i < file_count; i++) {
        free(file_list[i]);
    }

    // 清理 SDMMC
    esp_sdmmc_card_deinit();
    ESP_LOGI(TAG, "程序结束"); // 信息日志：程序结束
}
