/**
  ******************************************************************************
  * @file    sensor_frame.c
 * @brief   F407HALL I2C3 polling module
  *
  *  5 路快照来源:
  *    [0] UART4 (idx=0) -> F103jinliao01 (0x01, 10B)
  *    [1] USART1 (idx=1) -> F103jinliao02 (0x02, 12B)
  *    [2] UART5 (idx=3) -> F103chuliao05 (0x05, 12B)
  *    [3] I2C3           -> F407HALL (0x03, 16B)
  *    [4] USART2 (idx=2) -> F407HALL03 (0x04, 17B)
  *
  *  本文件负责 I2C3 轮询 (SensorFrame_PollI2C3),
  *  其余 4 路 UART 数据由 usart_protocol.c 直接处理.
  *  g_sensor_frame 快照已删除 (死代码, NMEA 输出直接从 g_*_data 读取).
  ******************************************************************************
  */
#include "sensor_frame.h"
#include "usart_protocol.h"
#include "bsp_i2c3_hall.h"

/* SPI1 read and parse; downstream data structure remains unchanged. */
void SensorFrame_PollI2C3(void)
{
    uint8_t frame[FRAME_HALL_VL6180X_LEN] = {0};
    uint8_t frame_ok = 0U;

    if (I2C3_HALL_ReadFrame(frame)) {
        frame_ok = Protocol_ParseHallVL6180XFrame(frame);
    }

    if (!frame_ok && g_rx_frame_valid_hall_vl6180x) {
        Protocol_ExpireStaleData(HAL_GetTick(), 500U);
    }
}

/* ================== 初始化 (占位, 保留接口兼容) ================== */
void SensorFrame_Init(void)
{
    /* 无需初始化, 解析器全局变量已 zero-init */
}
