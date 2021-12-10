# gummy

X11 CLI screen manager.

Allows automatic and manual brightness adjustment, via backlight (currently only for embedded displays) or gamma as a fallback.

Automatic screen brightness is based on screen contents. Possibly in the future I'll add support for ambient light sensors.

It also allows automatic and manual temperature.

# Build

Requirements:

- X11
- XCB
- XCB-Randr
- XLib-Shm
- sdbus-c++
 
```
git clone https://github.com/Fushko/gummy.git
cmake .
cmake --build .
sudo make install
```

The installation will put `90-backlight.rules` in `/usr/lib/udev/rules.d`.

This is a udev rule that allows users in the `video` group to apply backlight adjustments without root priviliges.

You also need to add your user to the `video` group, then restart:

```
usermod -a -G video {YOUR_USERNAME}
```

If the `video` group is not present, add it:

```
sudo groupadd video
```
