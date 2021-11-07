# Plant watering system

## Getting started:

### Create a config file:

`arduino-cli config init`

### Install esp32 core:

Change section in `~/.arduino15/arduino-cli.yaml` to look like the following:

```
board_manager:
  additional_urls:
    - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

`arduino-cli core update-index`

`arduino-cli core install esp32:esp32`

### Install additional libraries:

`arduino-cli lib install PageBuilder "Adafruit ADS1X15" AutoConnect`

### Compile:

`make compile`

### Upload:

`make upload`

The fqdn `esp32:esp32:nodemcu-32s` works for the ESP32-cam too.
If necessary, replace `/dev/ttyUSB0` in the Makefile with the serial port your ESP is connected to.

### Monitor:

`make monitor`

Feel free to use a different serial monitor than picocom but keep the same baud rate.

