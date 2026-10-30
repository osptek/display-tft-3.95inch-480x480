# Raspberry Pi 5 · ST7102 480×480（仅显示）

本目录为 3.95″ ST7102 MIPI 模组在 Raspberry Pi 5 上的 **仅显示** 内核模块与 DT overlay 示例（无触摸）。

本目录文件：

| 文件 | 说明 |
| ---- | ---- |
| `panel-st7102-480x480.c` | ST7102 DRM panel 驱动 |
| `Makefile` | 内核模块编译 |
| `vc4-kms-dsi-st7102-480x480.dts` | DSI overlay |

---

# 1. 准备工作

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) device-tree-compiler
```

将本目录拷到树莓派后进入该目录（以下命令默认在本目录执行）。

# 2. 编译内核模块

```bash
make clean
make
```

# 3. 编译并安装 Overlay / 模块

```bash
dtc -@ -I dts -O dtb -o vc4-kms-dsi-st7102-480x480.dtbo vc4-kms-dsi-st7102-480x480.dts
sudo cp vc4-kms-dsi-st7102-480x480.dtbo /boot/firmware/overlays/
sudo mkdir -p /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/panel/
sudo cp panel-st7102-480x480.ko /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/panel/
sudo depmod -a
```

> Overlay 必须使用 `dtc -@`，否则符号修复可能失败，DTO 无法正确加载。

# 4. 启用

编辑 `/boot/firmware/config.txt`：

```bash
sudo nano /boot/firmware/config.txt
```

添加（或确认）：

```text
# 关闭自动检测，避免和手动 overlay 冲突
display_auto_detect=0

dtoverlay=vc4-kms-v3d

dtoverlay=vc4-kms-dsi-st7102-480x480

# 忽略官方 LCD
ignore_lcd=1
```

重启：

```bash
sudo reboot
```
