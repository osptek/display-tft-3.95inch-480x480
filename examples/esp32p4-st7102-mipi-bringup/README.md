# ESP32-P4 · ST7102 MIPI + ST7123 触摸 bringup

面向 **YDP395BT008-V1**（In-Cell）的点亮例程：ESP32-P4 + ST7102（MIPI DSI）+ ST7123 触摸 + LVGL9，并带 Modbus UI 演示。

## 依赖

- ESP-IDF（建议 5.x，见 `main/idf_component.yml`）
- 组件：`esp_lvgl_port`、`lvgl`、`esp_lcd_touch_st7123`、`esp-modbus`（由 Component Manager 拉取）
- 本地组件：`components/esp_lcd_st7102`

## 编译与烧录

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> flash monitor
```

首次构建会下载 `managed_components/`（已在 `.gitignore` 中忽略）。

## 说明

- 分辨率 480×480；引脚与复位/背光/触摸 I2C 定义见 `main/main.c`
- 触摸驱动为 **ST7123**（`espressif/esp_lcd_touch_st7123`）
