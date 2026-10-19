/*
 * USB Gamepad -> Sega Master System (DB9) Adapter
 * 目标板: 任意 RP2040 开发板 (RP2040-Zero / Pico / 等)
 * USB 手柄接开发板原生 USB-C 口 (TinyUSB Host), 用 USB-A 母转 USB-C OTG 头
 *
 * 原理: SMS 手柄是无源电路, 按键就是把 DB9 对应针脚短接到地(GND, pin8)。
 *       本固件读取 USB 手柄 HID 报文, 然后把对应 GPIO 设为
 *       输出并拉低(等效于按下), 松开时设为输入(Hi-Z, 等效于释放)。
 *
 * 玩家分配: 第 1 个挂载的手柄 = P1, 第 2 个 = P2 (接 USB Hub)。
 * 调试: UART1 TX = GP20 @115200, 打印原始 HID 报文, 方便调整按键映射。
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "tusb.h"

// ---------------- SMS 输出引脚 ----------------
// 顺序: {上, 下, 左, 右, B1(pin6), B2(pin9)}
// 每个脚经 4.7k 电阻串联后接 DB9 (RP2040 不耐 5V, 电阻限流保护)
static const uint p1_pins[] = { 2, 3, 4, 5, 6, 7 };
static const uint p2_pins[] = { 8, 9, 10, 11, 12, 13 };

#define DEBUG_UART uart1
#define DEBUG_TX_PIN 20

typedef struct {
  bool u, d, l, r, b1, b2;
} sms_state_t;

#define MAX_REPORT 64
static uint8_t report_buf[MAX_REPORT];

// 玩家分配表: dev_addr -> 玩家号 (0 = 未分配)
static uint8_t player_of_addr[CFG_TUH_DEVICE_MAX + 1];

// ---------------- SMS 无源输出 ----------------
static inline void sms_write(uint pin, bool pressed) {
  if (pressed) {
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);        // 拉低 = 模拟按下(对地导通)
  } else {
    gpio_set_dir(pin, GPIO_IN); // 高阻 = 松开(由主机内部上拉保持高电平)
  }
}

static void apply_sms(uint8_t player, const sms_state_t *s) {
  const uint *pins = (player == 1) ? p1_pins : p2_pins;
  sms_write(pins[0], s->u);
  sms_write(pins[1], s->d);
  sms_write(pins[2], s->l);
  sms_write(pins[3], s->r);
  sms_write(pins[4], s->b1);
  sms_write(pins[5], s->b2);
}

static void release_player(uint8_t player) {
  sms_state_t s = {0};
  apply_sms(player, &s);
}

// ---------------- HID 报文解析 ----------------
#define VID_SONY 0x054C
#define PID_DS4  0x05C4
#define PID_DS4_2 0x09CC

// Boot Protocol 标准布局 (8BitDo 有线/SNES 类/多数小厂手柄):
//   byte0: bit4 上, bit5 下, bit6 左, bit7 右, bit0..3 = 按键1..4
//   byte1: bit0..3 = 按键5..8
//   byte2: X 轴 (int8), byte3: Y 轴 (int8)
static void parse_boot(const uint8_t *r, uint16_t len, sms_state_t *s) {
  if (len < 4) return;
  uint8_t b0 = r[0], b1 = r[1];
  int8_t x = (int8_t)r[2], y = (int8_t)r[3];
  s->u = (b0 & 0x10) || (y < -64);
  s->d = (b0 & 0x20) || (y >  64);
  s->l = (b0 & 0x40) || (x < -64);
  s->r = (b0 & 0x80) || (x >  64);
  s->b1 = (b0 & 0x01) || (b1 & 0x01);   // 键1(A/Cross) -> SMS 键1
  s->b2 = (b0 & 0x02) || (b1 & 0x02);   // 键2(B/Circle) -> SMS 键2
}

// DualShock 4 (USB) 布局: 32/64 字节报文
//   byte5: bit0..3 十字键(hat), byte6: bit1 Cross, bit2 Circle
static void parse_ds4(const uint8_t *r, uint16_t len, sms_state_t *s) {
  if (len < 7) return;
  uint8_t hat = r[5] & 0x0F;
  s->u = (hat == 0) || (hat == 1) || (hat == 7);
  s->d = (hat == 3) || (hat == 4) || (hat == 5);
  s->l = (hat == 5) || (hat == 6) || (hat == 7);
  s->r = (hat == 1) || (hat == 2) || (hat == 3);
  s->b1 = r[6] & 0x02;   // Cross
  s->b2 = r[6] & 0x04;   // Circle
}

static void handle_report(uint8_t dev_addr, const uint8_t *r, uint16_t len) {
  uint8_t player = player_of_addr[dev_addr];
  if (player == 0) return;

  // 调试输出: 原始报文, 抄下来可用来给新手柄做映射
  printf("P%d report(%d): ", player, len);
  for (uint16_t i = 0; i < len && i < 16; i++) printf("%02X ", r[i]);
  printf("\n");

  sms_state_t s = {0};
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  if ((vid == VID_SONY) && (pid == PID_DS4 || pid == PID_DS4_2)) {
    parse_ds4(r, len, &s);
  } else {
    parse_boot(r, len, &s);
  }
  apply_sms(player, &s);
}

// ---------------- TinyUSB Host 回调 ----------------
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len) {
  (void)desc_report; (void)desc_len;

  // 分配玩家号
  for (uint8_t p = 1; p <= 2; p++) {
    bool taken = false;
    for (uint8_t a = 1; a <= CFG_TUH_DEVICE_MAX; a++)
      if (player_of_addr[a] == p) taken = true;
    if (!taken) { player_of_addr[dev_addr] = p; break; }
  }
  printf("HID mounted: addr=%d -> P%d\n", dev_addr, player_of_addr[dev_addr]);

  // 尝试切到 Boot Protocol (多数手柄支持; 不支持会自动回退)
  tuh_hid_set_protocol(dev_addr, instance, HID_ITF_PROTOCOL_BOOT);
  tuh_hid_receive_report(dev_addr, instance, report_buf, sizeof(report_buf));
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  (void)instance;
  release_player(player_of_addr[dev_addr]);
  player_of_addr[dev_addr] = 0;
  printf("HID unmounted: addr=%d\n", dev_addr);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const *report, uint16_t len) {
  handle_report(dev_addr, report, len);
  tuh_hid_receive_report(dev_addr, instance, report_buf, sizeof(report_buf));
}

// ---------------- Main: 原生 USB 口做 Host ----------------
int main(void) {
  stdio_uart_init();   // 注意: 不用 USB 串口, USB 口整口给 Host 用

  // 调试串口 UART1: TX = GP20 (RX 不用接)
  uart_init(DEBUG_UART, 115200);
  uart_set_tx(DEBUG_UART, DEBUG_TX_PIN);

  // SMS 输出脚: 初始全部为高阻(输入) = 无按键按下
  for (int i = 0; i < 6; i++) {
    gpio_init(p1_pins[i]); gpio_set_dir(p1_pins[i], GPIO_IN);
    gpio_init(p2_pins[i]); gpio_set_dir(p2_pins[i], GPIO_IN);
  }

  tuh_init(0);   // RP2040 原生 USB 控制器作为 Host

  printf("SMS USB adapter ready (native USB host).\n");
  while (true) {
    tuh_task();
    tight_loop_contents();
  }
}
