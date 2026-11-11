# Raspberry Pi 5 · ST7102 显示 + ST7123 触摸（480×480）

本目录为 3.95″ ST7102 MIPI 模组在 Raspberry Pi 5 上的 **显示 + 触摸** 内核模块与 DT overlay 示例。

本目录文件：

| 文件 | 说明 |
| ---- | ---- |
| `panel-st7102-480x480.c` | ST7102 DRM panel 驱动 |
| `st7123_touch.c` | ST7123 触摸驱动 |
| `Makefile` | 内核模块编译 |
| `vc4-kms-dsi-st7102-480x480-st7123-overlay.dts` | DSI + 触摸 overlay |

若还需跑 LVGL，见同级目录 [`../rpi5-lvgl-st7102-st7123-480x480/`](../rpi5-lvgl-st7102-st7123-480x480/)。

---

# 1. 准备工作

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) device-tree-compiler
```

将本目录拷到树莓派后进入该目录。

# 2. 编译内核模块

```bash
make clean
make
```

# 3. 安装模块

```bash
sudo mkdir -p /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/panel/
sudo cp panel-st7102-480x480.ko /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/panel/

sudo mkdir -p /lib/modules/$(uname -r)/kernel/drivers/input/touchscreen/
sudo cp st7123_touch.ko /lib/modules/$(uname -r)/kernel/drivers/input/touchscreen/

sudo depmod -a
```

开机自动加载：编辑 `/etc/modules`，末尾加上：

```text
panel-st7102-480x480
st7123_touch
```

# 4. 编译并安装 Overlay

```bash
dtc -@ -I dts -O dtb -o vc4-kms-dsi-st7102-480x480-st7123-overlay.dtbo vc4-kms-dsi-st7102-480x480-st7123-overlay.dts
sudo cp vc4-kms-dsi-st7102-480x480-st7123-overlay.dtbo /boot/firmware/overlays/
```

> Overlay 必须使用 `dtc -@`，否则符号修复可能失败，DTO 无法正确加载。

# 5. 启用

编辑 `/boot/firmware/config.txt`：

```bash
sudo nano /boot/firmware/config.txt
```

```text
# 关闭自动检测，避免和手动 overlay 冲突
display_auto_detect=0

dtoverlay=vc4-kms-v3d

# 启用 ST7102 显示与 ST7123 触控 Overlay
dtoverlay=vc4-kms-dsi-st7102-480x480-st7123-overlay

# 忽略官方 LCD
ignore_lcd=1
```

重启：

```bash
sudo reboot
```
