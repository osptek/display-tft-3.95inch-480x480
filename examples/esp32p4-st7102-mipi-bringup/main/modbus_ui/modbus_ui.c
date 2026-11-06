#include "modbus_ui.h"

#include "modbus_port.h"
#include "lvgl.h"

#define MODBUS_UI_REFRESH_MS 200
#define MODBUS_UI_COLS         5
#define MODBUS_UI_ROWS         2

static const uint32_t s_header_colors[] = {
    0x1565C0, 0x2E7D32, 0xEF6C00, 0x6A1B9A, 0xC62828,
};

static lv_obj_t *s_value_labels[MODBUS_HOLDING_REG_COUNT];

static lv_obj_t *modbus_ui_create_cell(lv_obj_t *parent, size_t index)
{
    const uint32_t accent = s_header_colors[index % 5];

    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, 86, 148);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cell, 14, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(accent), 0);
    lv_obj_set_style_border_width(cell, 2, 0);
    lv_obj_set_style_shadow_color(cell, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_shadow_width(cell, 12, 0);
    lv_obj_set_style_shadow_opa(cell, LV_OPA_40, 0);
    lv_obj_set_style_clip_corner(cell, true, 0);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cell, 0, 0);

    lv_obj_t *header = lv_obj_create(cell);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, 40);
    lv_obj_set_style_bg_color(header, lv_color_hex(accent), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lv_obj_t *name = lv_label_create(header);
    lv_label_set_text_fmt(name, "Reg[%u]", (unsigned)index);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_center(name);

    lv_obj_t *body = lv_obj_create(cell);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_top(body, 22, 0);
    lv_obj_set_style_pad_bottom(body, 10, 0);

    s_value_labels[index] = lv_label_create(body);
    lv_label_set_text_fmt(s_value_labels[index], "%u", (unsigned)modbus_port_get_holding(index));
    lv_obj_set_style_text_color(s_value_labels[index], lv_color_hex(accent), 0);
    lv_obj_set_style_text_font(s_value_labels[index], &lv_font_montserrat_28, 0);
    lv_obj_center(s_value_labels[index]);

    return cell;
}

static void modbus_ui_refresh_cb(lv_timer_t *timer)
{
    (void)timer;
    char buf[16];

    for (size_t i = 0; i < MODBUS_HOLDING_REG_COUNT; ++i) {
        lv_snprintf(buf, sizeof(buf), "%u", (unsigned)modbus_port_get_holding(i));
        lv_label_set_text(s_value_labels[i], buf);
    }
}

void modbus_ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF5F7FA), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Modbus Holding Regs");
    lv_obj_set_style_text_color(title, lv_color_hex(0x1A237E), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "ID=1  115200 8N1  UART1 TX53/RX51/DE52");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x546E7A), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 42);

    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 468, 330);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(grid, 18, 0);

    for (int row = 0; row < MODBUS_UI_ROWS; ++row) {
        lv_obj_t *row_obj = lv_obj_create(grid);
        lv_obj_remove_style_all(row_obj);
        lv_obj_set_size(row_obj, LV_PCT(100), 148);
        lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_obj, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        for (int col = 0; col < MODBUS_UI_COLS; ++col) {
            size_t index = (size_t)(row * MODBUS_UI_COLS + col);
            modbus_ui_create_cell(row_obj, index);
        }
    }

    lv_timer_create(modbus_ui_refresh_cb, MODBUS_UI_REFRESH_MS, NULL);
}
