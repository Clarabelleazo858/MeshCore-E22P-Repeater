# Releasing firmware

Create a tag such as `e22p-v1.16.0-1`. GitHub Actions builds:

- `firmware.bin` for OTA updates;
- `firmware-merged.bin` for first installation at offset `0x0`.

Before tagging, run:

```bash
pio run -e Xiao_S3_E22P_repeater
pio run -e Xiao_S3_E22P_repeater -t mergebin
```

Do not attach flash dumps, NVS partitions, credentials, identities or logs to a
release.
