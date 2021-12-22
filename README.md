# gummy

CLI screen manager for X11 that allows automatic and manual brightness/temperature adjustments, via backlight (currently only for embedded displays) and gamma. Multiple monitors are supported.

Automatic brightness is based on screen contents. Support for ambient light sensors is planned.

Disclaimer: this program is still in development! You are welcome and encouraged to submit bug reports, suggestions and pull requests.

## Installation

DEB packages are provided in each new [release.](https://github.com/Fushko/gummy/releases) RPM packages to be added in the future.

A reboot is required after the installation to ensure udev rules for backlight adjustments are loaded.

## Building from source

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

## Usage

Type `gummy -h` to print a help message.

Quick guide:

`gummy start` starts the daemon responsible for screen adjustments.

`gummy -B 1` enables auto brightness on all screens.

`gummy -B 0 -s 1` disables automatic brightness on the second screen.

`gummy -b 60 -s 1` sets the brightness to 60% on the second screen.

`gummy -t 3400` sets the temperature to 3400K on all screens.

`gummy -T 1 -y 06:00 -u 16:30` enables auto temperature on all screens, with the sunrise set to 06:00 and sunset to 16:30.

## Donations

You can buy me a coffee at: https://coindrop.to/fushko

## License

Copyright (C) Francesco Fusco. All rights reserved.

[GPL3](https://github.com/Fushko/gummy/blob/master/LICENSE)


