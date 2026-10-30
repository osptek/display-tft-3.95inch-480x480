```
ffmpeg -i input.mp4 -vf "fps=30,scale=-1:480:flags=lanczos,crop=480:in_h:(in_w-480)/2:0" -q:v 5 mjpeg_480_480_30fps.mjpeg
```

# 注意事项

>+ 启动PSRAM

>+ 宽和高是16倍数

# 支持长文件名
```
FATFS_LONG_FILENAMES=CONFIG_FATFS_LFN_HEAP
FATFS_MAX_LFN=255
```