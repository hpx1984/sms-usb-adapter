# USB 手柄 → 世嘉 SMS 转换器（RP2040 原生 USB Host 版）
适用任何 RP2040 开发板（RP2040-Zero / Raspberry Pi Pico 等）。
USB 手柄通过 **USB-A母 → USB-C(OTG) 头** 接开发板原生 USB 口，无需 PIO-USB 飞线。

## 原理
SMS 手柄是无源电路：按键 = 把 DB9 对应针脚短接到 GND(pin8)，没有任何协议。
固件读取 USB 手柄 HID 报文 → 对应 GPIO 拉低模拟按下 → 松开时设为高阻。

## 接线（详见 wiring.png）
| 功能 | DB9 针 | 接开发板 |
|---|---|---|
| 上 | 1 | GP2 串 4.7kΩ |
| 下 | 2 | GP3 串 4.7kΩ |
| 左 | 3 | GP4 串 4.7kΩ |
| 右 | 4 | GP5 串 4.7kΩ |
| 键1 | 6 | GP6 串 4.7kΩ |
| 键2 | 9 | GP7 串 4.7kΩ |
| GND | 8 | GND |
| +5V(可选供电) | 7 | 开发板 5V/VBUS 焊盘 |
| 空 | 5 | 不接 |

玩家 2（可选）：GP8~GP13 接第二个 DB9，对应关系相同。
手柄：USB-A母转USB-C OTG 头插开发板 USB-C 口；接 USB Hub 可双人。

## 拿 UF2 固件的两种方式

### 方式 A：GitHub Actions 云端编译（无需装环境，推荐）
1. 把本文件夹推到一个 GitHub 仓库（`.github/workflows/build-uf2.yml` 已放好）。
2. Actions 页面等约 3 分钟，Artifacts 里下载 `sms_usb_adapter_uf2.zip`，解压得 `.uf2`。
3. 按住 BOOTSEL 插开发板，拖入 UF2 即完成烧录。

### 方式 B：本地编译
```bash
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk && git submodule update --init lib/tinyusb && cd -
export PICO_SDK_PATH=~/pico-sdk
mkdir build && cd build && cmake .. && make -j$(nproc)
# build/sms_usb_adapter.uf2
```

## 手柄兼容性
- **已适配**：标准 HID Boot 手柄（8BitDo 有线、SFC/SNES 类手柄、街机摇杆等）、DualShock 4（USB 有线）
- **不支持**：Xbox 360 / Xbox One 有线手柄（XInput 私有协议，非 HID）
- 映射不对的：接 UART1 TX=GP20 @115200 看原始报文，照 `main.c` 里 `parse_ds4()` 加一段 VID/PID 映射即可

## 注意
- SMS 的 Pause 是主机按钮，不在手柄协议里，映射不到手柄。
- Mark III 手柄口兼容，本适配器同样适用。
- 主机 7 脚取电约 100mA；接功耗大的手柄建议独立 USB 供电。
- RP2040-Zero 没有 BOOTSEL 按键时用镊子短接 TF 卡旁的 BOOT 焊盘再上电。
