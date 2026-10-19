// TinyUSB 主机配置 (RP2040 原生 USB Host)
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_MCU            OPT_MCU_RP2040
#define CFG_TUSB_OS             OPT_OS_NONE
#define CFG_TUSB_DEBUG          0
#define CFG_TUSB_DEBUG_PRINTF   printf

// ---------------- HOST ----------------
#define CFG_TUH_ENABLED         1
#define CFG_TUH_DEVICE_MAX      4      // Hub 下挂多个手柄
#define CFG_TUH_HUB             1      // 支持 USB Hub (双人必备)
#define CFG_TUH_HID             4      // HID 设备数
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_API_EDPT_XFER   1

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H_
