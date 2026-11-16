```
/home/jin/.espressif/tools/python/v5.5.5/venv/bin/python -m esptool --chip esp32s3 merge_bin \
  --flash_mode dio \
  --flash_freq 80m \
  --flash_size 16MB \
  --output esp32s3-idf5_st7701-rgb_osptek-boot-anim_lvgl9_0x0.bin \
  0x0 bootloader/bootloader.bin \
  0x8000 partition_table/partition-table.bin \
  0x10000 esp32s3-idf5_st7701-rgb_osptek-boot-anim_lvgl9.bin \
  0x410000 mmap_build/assets/efc/efc.bin
```