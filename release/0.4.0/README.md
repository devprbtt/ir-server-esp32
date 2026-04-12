# IR Server ESP32 Release 0.4.0

Artifacts in this folder:

- `firmware.bin`: application image
- `spiffs.bin`: SPIFFS filesystem image with the web UI

Build source:

- PlatformIO environment: `esp32dev`

Flash with PlatformIO:

```powershell
platformio run -e esp32dev -t upload
platformio run -e esp32dev -t uploadfs
```
