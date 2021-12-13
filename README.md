# gummy

X11 CLI screen manager.

Allows automatic and manual brightness adjustment, via backlight (currently only for embedded displays) or gamma as a fallback. Multiple monitors are supported.

Automatic screen brightness is based on screen contents. Possibly in the future I'll add support for ambient light sensors.

It also allows automatic and manual temperature.

# Build

Requirements:

- C++17 compiler
- X11
- XCB
- XCB-Randr
- XLib-Shm
- sdbus-c++

#### Apt packages

`sudo apt install libxext-dev libxcb-randr0-dev libsdbus-c++-dev`

### Installation

```
git clone https://github.com/Fushko/gummy.git
cd gummy
cmake .
cmake --build .
sudo make install
```

The installation will put `90-backlight.rules` in `/usr/lib/udev/rules.d`.

This is a udev rule that allows users in the `video` group to apply backlight adjustments without root priviliges. For this to work, you also need to add your user to the `video` group, then restart:

```
usermod -a -G video {YOUR_USERNAME}
```

If the `video` group is not present, add it:

```
sudo groupadd video
```
