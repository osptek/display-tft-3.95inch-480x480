#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "hw_init.h"
#include "esp_lv_adapter.h"
#include "lv_demos.h"

/* EFC 相关 */
#include "lv_efc.h"
#include "esp_lv_fs.h"
#include "esp_mmap_assets.h"
#include "mmap_generate_efc.h"

static const char *TAG = "main";

/* EFC player context */
static mmap_assets_handle_t s_assets;
static esp_lv_fs_handle_t s_fs_handle;
static lv_obj_t *s_efc_anim;
static lv_timer_t *s_finish_check_timer = NULL;

// 挂载 EFC 资产分区并注册 LVGL 文件系统
static esp_err_t mount_efc_assets(void)
{
    mmap_assets_config_t cfg = {
        .partition_label = "efc",
        .max_files = MMAP_EFC_FILES,
        .checksum = MMAP_EFC_CHECKSUM,
        .flags = { .mmap_enable = 1 },
    };

    ESP_RETURN_ON_ERROR(mmap_assets_new(&cfg, &s_assets), TAG, "mmap init failed");

    size_t file_count = mmap_assets_get_stored_files(s_assets);
    if (file_count == 0) {
        mmap_assets_del(s_assets);
        return ESP_ERR_NOT_FOUND;
    }

    fs_cfg_t fs_cfg = {
        .fs_letter = 'E',
        .fs_nums = file_count,
        .fs_assets = s_assets,
    };

    ESP_RETURN_ON_ERROR(esp_lv_adapter_fs_mount(&fs_cfg, &s_fs_handle), TAG, "fs mount failed");
    ESP_LOGI(TAG, "EFC assets mounted on drive E:");

    return ESP_OK;
}

// EFC 播放完成检查回调
static void efc_finish_check_cb(lv_timer_t *timer)
{
    if (!timer) {
        return;
    }

    lv_obj_t *efc_anim = (lv_obj_t *)lv_timer_get_user_data(timer);
    if (!efc_anim || !lv_efc_is_loaded(efc_anim)) {
        return;
    }

    int32_t current = lv_efc_get_current_frame(efc_anim);
    int32_t total = lv_efc_get_total_frames(efc_anim);
    bool loop_enabled = lv_efc_get_loop_enabled(efc_anim);

    // 不循环且已到最后一帧 → 切换到 widgets demo
    if (!loop_enabled && (current >= total - 1 || current < 0)) {
        ESP_LOGI(TAG, "EFC播放完成，启动 lv_demo_widgets");

        lv_timer_delete(timer);
        s_finish_check_timer = NULL;

        if (s_efc_anim) {
            lv_obj_delete(s_efc_anim);
            s_efc_anim = NULL;
        }

        lv_demo_widgets();
    }
}

/**
 * @brief 初始化并启动 EFC 开机动画
 */
static void start_efc_player(lv_display_t *disp)
{
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        return;
    }

    lv_obj_t *screen = lv_disp_get_scr_act(disp);

    s_efc_anim = lv_efc_create(screen);
    lv_obj_align(s_efc_anim, LV_ALIGN_CENTER, 0, 0);

    const char *file_name = mmap_assets_get_name(s_assets, 0);
    if (file_name) {
        char path[64];
        snprintf(path, sizeof(path), "E:%s", file_name);

        lv_efc_set_src(s_efc_anim, path);

        if (lv_efc_is_loaded(s_efc_anim)) {
            lv_efc_set_frame_delay(s_efc_anim, 20);
            lv_efc_set_loop_enabled(s_efc_anim, false);  // 只播一次

            s_finish_check_timer = lv_timer_create(efc_finish_check_cb, 100, s_efc_anim);
        } else {
            ESP_LOGW(TAG, "EFC load failed");
        }
    } else {
        ESP_LOGW(TAG, "No EFC file found");
    }

    esp_lv_adapter_unlock();
}

/* 独立任务：避免 EFC 解码占用 main 任务栈 */
static void efc_player_task(void *arg)
{
    lv_display_t *disp = (lv_display_t *)arg;
    start_efc_player(disp);
    vTaskDelete(NULL);
}

/**
 * @brief Get the configured display rotation from Kconfig
 *
 * @return esp_lv_adapter_rotation_t Rotation angle
 */
static esp_lv_adapter_rotation_t get_configured_rotation(void)
{
#if CONFIG_EXAMPLE_DISPLAY_ROTATION_0
    return ESP_LV_ADAPTER_ROTATE_0;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_90
    return ESP_LV_ADAPTER_ROTATE_90;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_180
    return ESP_LV_ADAPTER_ROTATE_180;
#elif CONFIG_EXAMPLE_DISPLAY_ROTATION_270
    return ESP_LV_ADAPTER_ROTATE_270;
#else
    return ESP_LV_ADAPTER_ROTATE_0;
#endif
}

void app_main()
{
    esp_lcd_panel_handle_t display_panel = NULL;
    esp_lcd_panel_io_handle_t display_io_handle = NULL;
    esp_lv_adapter_rotation_t rotation = get_configured_rotation();
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode;
    esp_lv_adapter_display_config_t display_config;

    tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    ESP_LOGI(TAG, "Selected LCD interface: RGB");

    /* Initialize the LCD hardware panel */
    ESP_LOGI(TAG, "Initializing LCD: %dx%d", HW_LCD_H_RES, HW_LCD_V_RES);
    ESP_ERROR_CHECK(hw_lcd_init(&display_panel, &display_io_handle, tear_avoid_mode, rotation));

    /* Initialize the LVGL adapter */
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                         display_panel, display_io_handle, HW_LCD_H_RES, HW_LCD_V_RES, rotation);

    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to register display");
        return;
    }

    lv_obj_t *default_scr = lv_disp_get_scr_act(disp);
    if (default_scr) {
        lv_obj_remove_style_all(default_scr);
        lv_obj_set_style_bg_color(default_scr, lv_color_black(), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(default_scr, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_size(default_scr, LV_PCT(100), LV_PCT(100));
    }

    /* Initialize input device based on interface type */
#if HW_USE_TOUCH
    ESP_LOGI(TAG, "Initializing touch panel");
    esp_lcd_touch_handle_t touch_handle = NULL;
    ESP_ERROR_CHECK(hw_touch_init(&touch_handle, rotation));

    /* Use the default config macro for quick setup with 1:1 coordinate scaling */
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch_handle);
    lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
    if (touch == NULL) {
        ESP_LOGE(TAG, "Failed to register touch");
        return;
    }
#endif

    /* Start the LVGL adapter */
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    // 挂载 EFC 资产，并在独立任务中启动开机动画
    ESP_ERROR_CHECK(mount_efc_assets());
    xTaskCreatePinnedToCore(efc_player_task, "efc_player", 16384, disp, 5, NULL, 0);

    ESP_LOGI(TAG, "系统运行中，等待 EFC 播放完成后进入 widgets demo...");
}
