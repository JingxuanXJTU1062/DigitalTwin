/**
  ******************************************************************************
  * @file    wifi_cmd.c
  * @brief   主机 -> F407 命令接收模块实现
  *
  *  完整流程:
  *    PC (mcu_wifi_listener.py)
  *      legacy $CMD parser; production control now uses Modbus coils
  *      --[USART3 +IPD,<id>,<len>:$CMD,...*CS\r\n]--> F407
  *      USART3 ISR:  push 到 strEsp8266_Fram_Record (现有, AT命令)
  *                  + wifi_cmd_push_byte() (本模块, 解析 PC 下发的 $CMD 帧)
  *      main loop:    wifi_cmd_process() -> drain -> 解析 $CMD 帧 -> 设置 GPIO
  *
  *  重要: ESP8266 默认 AT+CIPDINFO=1, 在每个透传包前附加 "+IPD,<id>,<len>:" 头.
  *        本模块采用"找 '$' 才开启新帧"的策略跳过该头.
  ******************************************************************************
  */
#include "wifi_cmd.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============== GPIO 引脚定义 ==============
 *
 * 避开所有 USART 引脚 (PA9/PA10 USART1, PB10/PB11 USART3, PC10/PC11 UART4)
 * 避开所有可能的 I2C 引脚 (PB6/7/8/9 I2C1, PB10/11 I2C2, PA9/10 I2C2-remap,
 *                         PA8/PC9 I2C3, PH7/8 I2C3-remap)
 * 避开已被占用的引脚: PE2 (ESP8266 CH_PD), PE3 (DHT11 DATA),
 *                    PF6/7/8 (RGB LED), PG15 (ESP8266 RST)
 *
 * 选择 PE4 / PE5 / PE6 / PC2，PC2 用于新增的出料控制。
 */
#define WIFI_CMD_PORT          GPIOE
#define WIFI_CMD_CLK_ENABLE()  __HAL_RCC_GPIOE_CLK_ENABLE()
#define WIFI_CMD_PIN1          GPIO_PIN_4   /* flag1 -> PE4 */
#define WIFI_CMD_PIN2          GPIO_PIN_5   /* flag2 -> PE5 */
#define WIFI_CMD_PIN3          GPIO_PIN_6   /* flag3 -> PE6 */
#define WIFI_CMD_PORT4         GPIOC
#define WIFI_CMD_CLK4_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define WIFI_CMD_PIN4          GPIO_PIN_2   /* flag4 -> PC2 */

/* ============== Watch 变量 (Keil Watch 窗口实时观测) ============== */
volatile uint8_t  g_cmd_flag1      = 0;
volatile uint8_t  g_cmd_flag2      = 0;
volatile uint8_t  g_cmd_flag3      = 0;
volatile uint8_t  g_cmd_flag4      = 0;
volatile uint32_t g_cmd_rx_count    = 0;
volatile uint32_t g_cmd_rx_bad_cs   = 0;
volatile uint32_t g_cmd_rx_overflow = 0;
volatile uint32_t g_cmd_rx_total    = 0;   /* ISR push 进 ring 的总字节数 (含 ESP8266 头部) */

/* ============== Ring buffer (ISR safe, single-producer/single-consumer) ==============
 * 256 字节足够: "$CMD,f1,f2,f3,f4*CS\r\n" 最多约 26 字节, ring 足够容纳多条
 */
#define WIFI_CMD_RING_SIZE   256
typedef struct {
    uint8_t  data[WIFI_CMD_RING_SIZE];
    volatile uint16_t head;   /* ISR 写 */
    volatile uint16_t tail;   /* main 读 */
} WifiCmdRing_TypeDef;

static WifiCmdRing_TypeDef s_cmd_ring = { {0}, 0, 0 };

/* ============== 帧解析状态机 ============== */
#define WIFI_CMD_FRAME_MAX    32   /* $CMD,...*CS\r\n 上限 */
static char     s_frame_buf[WIFI_CMD_FRAME_MAX + 1];
static uint16_t s_frame_len     = 0;

/* ============== 设置单个 GPIO 电平 ============== */
static inline void wifi_cmd_set_pin(GPIO_TypeDef *port, uint16_t pin, uint8_t level)
{
    HAL_GPIO_WritePin(port, pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ============== 把解析后的 4 个 flag 应用到 GPIO + Watch 变量 ============== */
static void wifi_cmd_apply_flags(uint8_t f1, uint8_t f2, uint8_t f3, uint8_t f4)
{
    g_cmd_flag1 = (f1 ? 1u : 0u);
    g_cmd_flag2 = (f2 ? 1u : 0u);
    g_cmd_flag3 = (f3 ? 1u : 0u);
    g_cmd_flag4 = (f4 ? 1u : 0u);

    wifi_cmd_set_pin(WIFI_CMD_PORT, WIFI_CMD_PIN1, g_cmd_flag1);
    wifi_cmd_set_pin(WIFI_CMD_PORT, WIFI_CMD_PIN2, g_cmd_flag2);
    wifi_cmd_set_pin(WIFI_CMD_PORT, WIFI_CMD_PIN3, g_cmd_flag3);
    wifi_cmd_set_pin(WIFI_CMD_PORT4, WIFI_CMD_PIN4, g_cmd_flag4);
}

void wifi_cmd_set_output(uint8_t index, uint8_t level)
{
    level = level ? 1U : 0U;
    if (index == 0U) {
        g_cmd_flag1 = level;
        wifi_cmd_set_pin(WIFI_CMD_PORT, WIFI_CMD_PIN1, level);
    } else if (index == 1U) {
        g_cmd_flag2 = level;
        wifi_cmd_set_pin(WIFI_CMD_PORT, WIFI_CMD_PIN2, level);
    } else if (index == 2U) {
        g_cmd_flag3 = level;
        wifi_cmd_set_pin(WIFI_CMD_PORT, WIFI_CMD_PIN3, level);
    } else if (index == 3U) {
        g_cmd_flag4 = level;
        wifi_cmd_set_pin(WIFI_CMD_PORT4, WIFI_CMD_PIN4, level);
    }
}

/* ============== 解析 1 行 $CMD 帧 ============== */
static void wifi_cmd_handle_line(const char *line)
{
    /* line: "$CMD,<f1>,<f2>,<f3>,<f4>*CS" (已剥掉 \r\n) */
    if (line[0] != '$') return;
    const char *body = line + 1;

    const char *star = strchr(body, '*');
    if (!star) return;

    /* 校验和: body[0..star-body-1] 的 XOR == 解析出的 hex */
    if (star[1] == '\0' || star[2] == '\0') return;   /* 校验和长度不够 */

    uint8_t cs_recv = 0;
    {
        char hex[3] = { star[1], star[2], 0 };
        char *endp = 0;
        unsigned long v = strtoul(hex, &endp, 16);
        if (endp == hex || *endp != 0) return;
        cs_recv = (uint8_t)v;
    }

    /* 计算范围: body[0] 到 '*' 之前, 即 [body, star) */
    uint8_t cs_calc = 0;
    {
        const char *p = body;
        while (p < star) cs_calc ^= (uint8_t)*p++;
    }

    if (cs_recv != cs_calc) {
        g_cmd_rx_bad_cs++;
        return;
    }

    /* 解析 msg: body = "CMD,<f1>,<f2>,<f3>,<f4>" */
    if (strncmp(body, "CMD,", 4) != 0) return;

    uint8_t f1 = 0, f2 = 0, f3 = 0, f4 = 0;
    {
        /* 用临时 buffer 复制 body 前缀部分以便 sscanf 安全 */
        char tmp[24];
        uint16_t body_len = (uint16_t)(star - body);
        if (body_len >= sizeof(tmp)) return;
        memcpy(tmp, body, body_len);
        tmp[body_len] = 0;
        int i1 = 0, i2 = 0, i3 = 0, i4 = 0;
        char trailing = 0;
        if (sscanf(tmp, "CMD,%d,%d,%d,%d%c", &i1, &i2, &i3, &i4,
                   &trailing) != 4) return;
        if (i1 < 0 || i1 > 1 || i2 < 0 || i2 > 1 ||
            i3 < 0 || i3 > 1 || i4 < 0 || i4 > 1) return;
        f1 = (uint8_t)i1;
        f2 = (uint8_t)i2;
        f3 = (uint8_t)i3;
        f4 = (uint8_t)i4;
    }

    wifi_cmd_apply_flags(f1, f2, f3, f4);
    g_cmd_rx_count++;
}

/* ============== API: 初始化 ============== */
void wifi_cmd_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    WIFI_CMD_CLK_ENABLE();
    WIFI_CMD_CLK4_ENABLE();

    GPIO_InitStruct.Pin   = WIFI_CMD_PIN1 | WIFI_CMD_PIN2 | WIFI_CMD_PIN3;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(WIFI_CMD_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = WIFI_CMD_PIN4;
    HAL_GPIO_Init(WIFI_CMD_PORT4, &GPIO_InitStruct);

    /* 初始化为低电平 */
    wifi_cmd_apply_flags(0, 0, 0, 0);

    /* 清 ring buffer 和解析状态 */
    s_cmd_ring.head = 0;
    s_cmd_ring.tail = 0;
    s_frame_len     = 0;
}

/* ============== API: ISR 推 1 字节 (O(1), 不阻塞) ============== */
void wifi_cmd_push_byte(uint8_t byte)
{
    uint16_t next = (uint16_t)((s_cmd_ring.head + 1u) % WIFI_CMD_RING_SIZE);
    if (next == s_cmd_ring.tail) {
        /* ring 满, 丢弃并计数 */
        g_cmd_rx_overflow++;
        return;
    }
    s_cmd_ring.data[s_cmd_ring.head] = byte;
    s_cmd_ring.head = next;
    g_cmd_rx_total++;
}

/* ============== API: main 循环调用 (非阻塞) ============== */
void wifi_cmd_process(void)
{
    /* 1 字节 1 字节消费 ring, 找 \n 结尾的完整帧
     *
     * ESP8266 默认 AT+CIPDINFO=1, 透传 PC 数据时会附加 "+IPD,<id>,<len>:" 头
     * F407 收到的字节流形如:
     *   +IPD,0,14:$CMD,1,0,1,0*4A\r\n...
     *
     * $CMD 帧以 '$' 开头, 所以采用"找 '$' 才开启新帧"的策略:
     *   - 见到任何非 '$' 的字节 (在找 '$' 阶段), 全部忽略
     *   - 见到 '$' 后开始累加到 s_frame_buf
     *   - 见到 \n 时处理
     */
    while (s_cmd_ring.tail != s_cmd_ring.head) {
        uint8_t b = s_cmd_ring.data[s_cmd_ring.tail];
        s_cmd_ring.tail = (uint16_t)((s_cmd_ring.tail + 1u) % WIFI_CMD_RING_SIZE);

        if (b == '\r') {
            /* 忽略 \r, 等真正的 \n 触发帧处理 (兼容 \r\n 与 \n) */
            continue;
        }

        /* 在找 '$' 阶段: 只忽略其他字节 (跳过 ESP8266 的 +IPD 头) */
        if (s_frame_len == 0 && b != '$') {
            continue;
        }

        if (b == '\n') {
            /* 一帧完整, 处理 */
            s_frame_buf[s_frame_len] = 0;
            wifi_cmd_handle_line(s_frame_buf);
            s_frame_len = 0;
            continue;
        }

        /* 累加到帧 buffer, 防止溢出 */
        if (s_frame_len < WIFI_CMD_FRAME_MAX) {
            s_frame_buf[s_frame_len++] = (char)b;
        } else {
            /* 帧太长, 丢弃并重新同步 */
            s_frame_len = 0;
        }
    }
}
