/*
 * ST7102 480x480 2-Lane MIPI-DSI Panel Driver for Raspberry Pi 5
 *
 * This example is from the open-source sharing by engineers of Yuying Optoelectronics (鱼鹰光电)
 * on Github.com/osptek. Welcome to provide improvement suggestions.
 *
 * 本例程来源于鱼鹰光电的工程师的开源分享 Github.com/osptek，欢迎提出改进意见
 */
 
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <video/mipi_display.h>

struct power_on_timing {
	unsigned long post_reset;
	unsigned long reset_low;
	unsigned long after_reset;
	unsigned long slpout;
};

struct st7102_desc {
	const struct drm_display_mode *mode;
	unsigned int lanes;
	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	int (*init_sequence)(struct mipi_dsi_device *dsi);
	const struct power_on_timing *pwr_timing;
	bool do_sw_reset;
};

struct st7102 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct st7102_desc *desc;
	struct gpio_desc *reset;
	enum drm_panel_orientation orientation;
};

static inline struct st7102 *to_st7102(struct drm_panel *panel)
{
	return container_of(panel, struct st7102, panel);
}

/* ==================== 更新后的 ST7102 480x480 初始化序列 ==================== */
static int st7102_480x480_init_sequence(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };

	/* Key Unlock */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x99, 0x71, 0x02, 0xa2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x99, 0x71, 0x02, 0xa3);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x99, 0x71, 0x02, 0xa4);

	/* VGH_VGL */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB0, 0x22, 0x61, 0x1E, 0x61, 0x2F, 0x39, 0x39);

	/* Source */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB7, 0x46, 0x46);

	/* VCOM */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xBF, 0x50, 0x50);

	/* GIP Settings */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xD7, 0x00, 0x0C, 0x80, 0x08, 0xF0, 0xF0);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xA3,
		0x40, 0x03, 0x80, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x04,
		0x6F, 0x6F, 0x00, 0x1A, 0x00, 0x45, 0x05, 0x00, 0x00, 0x00,
		0x00, 0x46, 0x00, 0x00, 0x12, 0x20, 0x52, 0x00, 0x05, 0x00,
		0x00, 0xFF);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xA6,
		0x08, 0x00, 0x24, 0x55, 0x35, 0x00, 0x76, 0x40, 0x58, 0x58,
		0x00, 0x24, 0x55, 0x00, 0x00, 0x40, 0x40, 0x58, 0x58, 0x02,
		0xAC, 0x51, 0x00, 0xCC, 0x40, 0x40, 0x58, 0x58, 0x00, 0xAC,
		0x11, 0x00, 0x00, 0x40, 0x40, 0x58, 0x58, 0x00, 0x00, 0x06,
		0x00, 0x00, 0x00, 0x00);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xA7,
		0x19, 0x19, 0x00, 0x64, 0x40, 0x07, 0x16, 0x40, 0x00, 0x44,
		0x43, 0x58, 0x58, 0x00, 0x64, 0x40, 0x25, 0x34, 0x00, 0x00,
		0x42, 0x41, 0x58, 0x58, 0x00, 0x64, 0x40, 0x4B, 0x5A, 0x00,
		0x00, 0x42, 0x41, 0x58, 0x58, 0x00, 0x24, 0x40, 0x69, 0x78,
		0x00, 0x00, 0x40, 0x40, 0x58, 0x58, 0x00, 0x44);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xAC,
		0x00, 0x1C, 0x04, 0x1A, 0x19, 0x1B, 0x1B, 0x18, 0x06, 0x13,
		0x19, 0x11, 0x1B, 0x08, 0x18, 0x0A, 0x01, 0x1C, 0x04, 0x1A,
		0x19, 0x01, 0x1B, 0x18, 0x06, 0x12, 0x19, 0x10, 0x1B, 0x09,
		0x18, 0x0B, 0xBF, 0xAA, 0xBF, 0xAA, 0x00);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xAD, 0xCC, 0x40, 0x46, 0x11, 0x04, 0x6F, 0x6F);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE8,
		0x30, 0x07, 0x05, 0x74, 0x74, 0x9C, 0x00, 0xE2, 0x04, 0x00,
		0x00, 0x00, 0x00, 0xEF);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0x75, 0x03, 0x04);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE7,
		0x8B, 0x3C, 0x00, 0x0C, 0xF0, 0x5D, 0x00, 0x5D, 0x00, 0x5D,
		0x00, 0x5D, 0x00, 0xFF, 0x00, 0x08, 0x7B, 0x00, 0x00, 0xC8,
		0x6A, 0x5A, 0x08, 0x1A, 0x3C, 0x00, 0xA1, 0x01, 0x8C, 0x01,
		0x7F, 0xF0, 0x22);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE9, 0x3C, 0x7F, 0x08, 0x07, 0x1A, 0x7A, 0x22, 0x1A, 0x33);

	/* Gamma */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC8,
		0x00, 0x00, 0x15, 0x26, 0x44, 0x00, 0x78, 0x03, 0xBE, 0x06,
		0x11, 0x1C, 0x09, 0x8A, 0x03, 0x21, 0xD4, 0x01, 0x11, 0x0F,
		0x22, 0x4A, 0x0F, 0x8F, 0x0A, 0x32, 0xF0, 0x0A, 0x41, 0x0D,
		0xF3, 0x80, 0x0D, 0xAE, 0xC5, 0x03, 0xC4);

	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC9,
		0x00, 0x00, 0x15, 0x26, 0x44, 0x00, 0x78, 0x03, 0xBE, 0x06,
		0x11, 0x1C, 0x09, 0x8A, 0x03, 0x21, 0xD4, 0x01, 0x11, 0x0F,
		0x22, 0x4A, 0x0F, 0x8F, 0x0A, 0x32, 0xF0, 0x0A, 0x41, 0x0D,
		0xF3, 0x80, 0x0D, 0xAE, 0xC5, 0x03, 0xC4);

	/* Exit Sleep & Delay 120ms */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x11);
	msleep(120);

	/* Display ON & Delay 20ms */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x29);
	msleep(20);

	return ctx.accum_err;
}

static int st7102_prepare(struct drm_panel *panel)
{
	struct st7102 *st7102 = to_st7102(panel);
	struct mipi_dsi_multi_context ctx = { .dsi = st7102->dsi };

	if (st7102->reset) {
		gpiod_set_value_cansleep(st7102->reset, 1);
		msleep(st7102->desc->pwr_timing->post_reset);
		gpiod_set_value_cansleep(st7102->reset, 0);
		msleep(st7102->desc->pwr_timing->reset_low);
		gpiod_set_value_cansleep(st7102->reset, 1);
		msleep(st7102->desc->pwr_timing->after_reset);
	}

	if (st7102->desc->do_sw_reset) {
		mipi_dsi_dcs_soft_reset_multi(&ctx);
		msleep(st7102->desc->pwr_timing->after_reset);
	}

	if (st7102->desc->init_sequence) {
		int ret = st7102->desc->init_sequence(st7102->dsi);
		if (ret)
			return ret;
	}

	return ctx.accum_err;
}

static int st7102_enable(struct drm_panel *panel)
{
	struct mipi_dsi_multi_context ctx = { .dsi = to_mipi_dsi_device(panel->dev) };
	mipi_dsi_dcs_set_display_on_multi(&ctx);
	return ctx.accum_err;
}

static int st7102_disable(struct drm_panel *panel)
{
	struct mipi_dsi_multi_context ctx = { .dsi = to_mipi_dsi_device(panel->dev) };
	mipi_dsi_dcs_set_display_off_multi(&ctx);
	return ctx.accum_err;
}

static int st7102_unprepare(struct drm_panel *panel)
{
	struct st7102 *st7102 = to_st7102(panel);
	struct mipi_dsi_multi_context ctx = { .dsi = st7102->dsi };

	mipi_dsi_dcs_enter_sleep_mode_multi(&ctx);
	if (st7102->reset)
		gpiod_set_value_cansleep(st7102->reset, 0);

	return ctx.accum_err;
}

static int st7102_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct st7102 *st7102 = to_st7102(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, st7102->desc->mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	drm_connector_set_orientation_from_panel(connector, panel);
	return 1;
}

static enum drm_panel_orientation st7102_get_orientation(struct drm_panel *panel)
{
	return to_st7102(panel)->orientation;
}

static const struct drm_panel_funcs st7102_funcs = {
	.prepare = st7102_prepare,
	.enable = st7102_enable,
	.disable = st7102_disable,
	.unprepare = st7102_unprepare,
	.get_modes = st7102_get_modes,
	.get_orientation = st7102_get_orientation,
};

/* ==================== 对应的 480x480 时序参数配置 ==================== */
static const struct drm_display_mode st7102_mode = {
	/* 30.8 MHz Pixel Clock */
	.clock = 30800,

	/* 水平时序 (Horizontal) */
	.hdisplay = 480,
	.hsync_start = 480 + 120,		/* HFP: 120 */
	.hsync_end   = 480 + 120 + 2,		/* HSW: 2 */
	.htotal      = 480 + 120 + 2 + 140,	/* HBP: 140 (HTOTAL = 742) */

	/* 垂直时序 (Vertical) */
	.vdisplay = 480,
	.vsync_start = 480 + 200,		/* VFP: 200 */
	.vsync_end   = 480 + 200 + 4,		/* VSW: 4 */
	.vtotal      = 480 + 200 + 4 + 8,	/* VBP: 8 (VTOTAL = 692) */

	/* 3.95寸 480x480 方屏实际显示区域物理尺寸 (单位: mm) */
	.height_mm = 72,
	.width_mm = 72,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static const struct power_on_timing st7102_pwr_timing = {
	.post_reset = 10,
	.reset_low = 10,
	.after_reset = 50,
	.slpout = 120,
};

static const struct st7102_desc st7102_desc = {
	.mode = &st7102_mode,
	.lanes = 2,
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = st7102_480x480_init_sequence,
	.pwr_timing = &st7102_pwr_timing,
	.do_sw_reset = true,
};

static int st7102_probe(struct mipi_dsi_device *dsi)
{
	struct st7102 *st7102;
	const struct st7102_desc *desc;
	int ret;

	st7102 = devm_kzalloc(&dsi->dev, sizeof(*st7102), GFP_KERNEL);
	if (!st7102)
		return -ENOMEM;

	desc = of_device_get_match_data(&dsi->dev);
	dsi->mode_flags = desc->flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

	st7102->panel.prepare_prev_first = true;
	st7102->reset = devm_gpiod_get_optional(&dsi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(st7102->reset)) {
		dev_err(&dsi->dev, "Failed to get reset GPIO\n");
		return PTR_ERR(st7102->reset);
	}

	ret = of_drm_get_panel_orientation(dsi->dev.of_node, &st7102->orientation);
	if (ret < 0)
		st7102->orientation = DRM_MODE_PANEL_ORIENTATION_NORMAL;

	drm_panel_init(&st7102->panel, &dsi->dev, &st7102_funcs, DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&st7102->panel);
	if (ret)
		return ret;

	drm_panel_add(&st7102->panel);

	mipi_dsi_set_drvdata(dsi, st7102);
	st7102->dsi = dsi;
	st7102->desc = desc;

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&st7102->panel);

	return ret;
}

static void st7102_remove(struct mipi_dsi_device *dsi)
{
	struct st7102 *st7102 = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&st7102->panel);
}

static const struct of_device_id st7102_of_match[] = {
	{ .compatible = "sitronix,st7102-480x480", .data = &st7102_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, st7102_of_match);

static struct mipi_dsi_driver st7102_driver = {
	.probe = st7102_probe,
	.remove = st7102_remove,
	.driver = {
		.name = "panel-st7102-480x480",
		.of_match_table = st7102_of_match,
	},
};
module_mipi_dsi_driver(st7102_driver);

MODULE_AUTHOR("Adapted for ST7102 480x480 2-Lane");
MODULE_DESCRIPTION("Sitronix ST7102 480x480 2-Lane MIPI-DSI Panel Driver");
MODULE_LICENSE("GPL");
