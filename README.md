# gummy

X11 CLI screen manager.

Allows automatic and manual brightness adjustment, via backlight (currently only for embedded displays) or gamma as a fallback. Multiple monitors are supported.

Automatic screen brightness is based on screen contents. Possibly in the future I'll add support for ambient light sensors.

It also allows automatic and manual temperature.

Disclaimer: this app is still in early development! You are welcome and encouraged to submit bug reports, suggestions and pull requests.

# Build

Requirements:

- C++17 compiler
- X11
- XCB
- XCB-Randr
- XLib-Shm
- sdbus-c++
- libudev

#### Apt packages

`sudo apt install build-essential cmake libxext-dev libxcb-randr0-dev libsdbus-c++-dev libudev-dev`

### Installation

```
git clone https://github.com/Fushko/gummy.git
cd gummy
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE="Release"
cmake --build .
sudo make install
```
A reboot is required to ensure udev rules that allow backlight adjustments are loaded.

# Usage

Type `gummy -h` to print the help text.

Quick guide:

`gummy start` starts the daemon responsible for screen adjustments.

`gummy -B 1` enables auto brightness on all screens.

`gummy -B 0 -s 1` disables automatic brightness on the second screen.

`gummy -b 60 -s 1` sets the brightness to 60% on the second screen.

`gummy -t 3400` sets the temperature to 3400K on all screens.

`gummy -T 1 -y 06:00 -u 16:30` enables auto temperature on all screens, with the sunrise set to 06:00 and sunset to 16:30.

# License

Copyright (C) Francesco Fusco. All rights reserved.

[GPL3](https://github.com/Fushko/gummy/blob/master/LICENSE)


