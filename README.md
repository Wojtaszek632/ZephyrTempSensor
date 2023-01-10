# ZephyrTempSensor
A weather station based on the Zephyr RTOS, Arduino Nano 33 BLE, and Android app
![Screenshot from 2023-01-07 22-38-19](https://user-images.githubusercontent.com/69490354/211171440-1915e9e7-5935-4b01-86e5-f174af5344e6.png)

```
source ~/zephyrproject/.venv/bin/activate
```

```
west build -p auto -b arduino_nano_33_ble ZephyrTempSensor -- -DDTC_OVERLAY_FILE=arduino_i2c.overlay
```

```
west flash --bossac=/home/wojciech/snap/arduino/85/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac
```
