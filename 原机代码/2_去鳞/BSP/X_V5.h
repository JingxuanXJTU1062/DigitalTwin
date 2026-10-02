#ifndef __X_V5_H
#define __X_V5_H

#include "usart.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * X 固件通讯命令封装（与 Emm_V5.c 中同名功能保持一致的“控制能力”）
 *
 * 说明：
 * - 绝大多数命令两种固件通用，见手册 5.1.2 说明：除少数命令外其余格式相同。fileciteturn1file1L13-L18
 * - 速度模式命令格式不同：X 固件速度模式为 Addr F6 dir acc(2B) speed(2B) sync 6B。fileciteturn2file1L11-L18
 *
 * 本文件仅封装“主机发送帧”，电机返回解析留给上层（原工程亦未做解析）。
 */

typedef enum {
  X_S_VER   = 0,   /* 读取固件版本和硬件版本 */
  X_S_RL    = 1,   /* 读取相电阻和相电感 */
  X_S_VBUS  = 3,   /* 读取总线电压 */
  X_S_CPHA  = 5,   /* 读取相电流 */
  X_S_ENCL  = 7,   /* 读取线性化编码器值 */
  X_S_TPOS  = 8,   /* 读取电机目标位置 */
  X_S_VEL   = 9,   /* 读取电机实时转速（X 固件单位为 0.1RPM，需要 /10）fileciteturn2file13L21-L24 */
  X_S_CPOS  = 10,  /* 读取电机实时位置 */
  X_S_PERR  = 11,  /* 读取电机位置误差 */
  X_S_FLAG  = 13,  /* 读取电机状态标志 */
  X_S_Conf  = 14,  /* 读取驱动配置参数 */
  X_S_State = 15,  /* 读取系统状态参数（X） */
  X_S_ORG   = 16,  /* 读取回零状态标志 */
} X_SysParams_t;

/* 与 Emm_V5.h 同名/同用途 API（但参数类型会根据 X 固件协议差异略有不同） */
void X_V5_Reset_CurPos_To_Zero(uint8_t addr);
void X_V5_Reset_Clog_Pro(uint8_t addr);
void X_V5_Read_Sys_Params(uint8_t addr, X_SysParams_t s);
void X_V5_Modify_Ctrl_Mode(uint8_t addr, bool svF, uint8_t ctrl_mode);
void X_V5_En_Control(uint8_t addr, bool state, bool snF);

/**
 * X 固件速度模式控制
 * @param dir    00/01：CW/CCW
 * @param acc    0-65535 RPM/S（两字节）fileciteturn2file0L3-L6
 * @param speed  0-30000（0.1RPM），即 0-3000.0RPM（两字节）fileciteturn2file0L3-L6
 * @param snF    同步标志 00/01（立即执行/先缓存）fileciteturn2file0L5-L6
 */
void X_V5_Vel_Control(uint8_t addr, uint8_t dir, uint16_t acc, uint16_t speed, bool snF);

/* 以下功能命令在手册中属于两种固件通用命令（或本工程仅用到“发送帧”即可） */
void X_V5_Stop_Now(uint8_t addr, bool snF);
void X_V5_Synchronous_motion(uint8_t addr);
void X_V5_Origin_Set_O(uint8_t addr, bool svF);
void X_V5_Origin_Modify_Params(uint8_t addr, bool svF, uint8_t o_mode, uint8_t o_dir, uint16_t o_vel, uint32_t o_tm,
                               uint16_t sl_vel, uint16_t sl_ma, uint16_t sl_ms, bool potF);
void X_V5_Origin_Trigger_Return(uint8_t addr, uint8_t o_mode, bool snF);
void X_V5_Origin_Interrupt(uint8_t addr);

#endif
