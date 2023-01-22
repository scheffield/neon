# Neon

Neon is a remote controllable RGB light meant for video shots and life streaming. It's build around the ESP32-S2 and addressable RGB lights like the WS2812 or SK6812. This repository contains all the files to build your own version.

If you haven't go check out the build and some more info in this YouTube video:

[![Neon YouTube video](https://i.ytimg.com/vi/w3ysH9Jxcy4/maxresdefault.jpg)](http://www.youtube.com/watch?v=w3ysH9Jxcy4 "Neon | PCB design, CAD and Code")

## Directory structure
```
$project_root/
├── cad/ ........... open with Fusion360
├── code/
│   ├── baords/ .... board config
│   ├── data/ ...... compiled website
│   ├── src/ ....... code for ESP32
│   ├── variants ... particion table
│   └── web/ ....... web UI
├── pcb/ ........... open with KiCad
└── stl/ ........... for 3D printing
```

## Web UI

### setup
```bash
cd web
npm i
```

### develop
```bash
npm run dev
```

### build
```bash
npm run webpack
cp dist/index.html ../data
pio run --target uploadfs
```

The last step is assuming that PlatformIO is installed and available as `pio`.

## BOM
Appart from the BOM for the PCB which can be exported from KiCad, the following parts are needed:

**BOM**
| Part | Amount |
|---|---|
| [1/4"*20 Threaded Heat Set Inserts](https://www.amazon.com.au/dp/B077P4M1CM) | 1 |
| [3-pin LiPo Battery 6000mAh](https://core-electronics.com.au/3-pin-lipo-battery-for-pijuice-6000mah.html) | 1 |