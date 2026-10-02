#include "X_V5.h"

/**
 * 说明：
 * - 本工程原 Emm_V5.c 里多数命令为通用格式（地址+功能码(+辅助码)+参数+6B），
 *   这里直接复用相同的帧格式，确保“能做的事情”一致。
 * - 唯一必须改的点：速度模式 F6 在 X 固件参数格式不同。fileciteturn2file1L11-L18
 */

void X_V5_Reset_CurPos_To_Zero(uint8_t addr)
{
  uint8_t cmd[4] = { addr, 0x0A, 0x6D, 0x6B };
  usart_SendCmd(cmd, sizeof(cmd));
}

void X_V5_Reset_Clog_Pro(uint8_t addr)
{
  uint8_t cmd[4] = { addr, 0x0E, 0x52, 0x6B };
  usart_SendCmd(cmd, sizeof(cmd));
}

void X_V5_Read_Sys_Params(uint8_t addr, X_SysParams_t s)
{
  uint8_t i = 0;
  uint8_t cmd[8] = {0};

  cmd[i++] = addr;

  switch(s)
  {
    case X_S_VER  : cmd[i++] = 0x1F; break;
    case X_S_RL   : cmd[i++] = 0x20; break;
    case X_S_VBUS : cmd[i++] = 0x24; break;
    case X_S_CPHA : cmd[i++] = 0x27; break;
    case X_S_ENCL : cmd[i++] = 0x31; break;
    case X_S_TPOS : cmd[i++] = 0x33; break;
    case X_S_VEL  : cmd[i++] = 0x35; break;
    case X_S_CPOS : cmd[i++] = 0x36; break;
    case X_S_PERR : cmd[i++] = 0x37; break;
    case X_S_FLAG : cmd[i++] = 0x3A; break;
    case X_S_ORG  : cmd[i++] = 0x3B; break;
    case X_S_Conf : cmd[i++] = 0x42; cmd[i++] = 0x6C; break;
    case X_S_State: cmd[i++] = 0x43; cmd[i++] = 0x7A; break; /* X 固件读取系统状态参数 43 7A 6B fileciteturn2file8L13-L16 */
    default: break;
  }

  cmd[i++] = 0x6B;
  usart_SendCmd(cmd, i);
}

void X_V5_Modify_Ctrl_Mode(uint8_t addr, bool svF, uint8_t ctrl_mode)
{
  uint8_t cmd[6] = {0};
  cmd[0] = addr;
  cmd[1] = 0x46;
  cmd[2] = 0x69;
  cmd[3] = (uint8_t)svF;
  cmd[4] = ctrl_mode;
  cmd[5] = 0x6B;
  usart_SendCmd(cmd, sizeof(cmd));
}

void X_V5_En_Control(uint8_t addr, bool state, bool snF)
{
  uint8_t cmd[6] = {0};
  cmd[0] = addr;
  cmd[1] = 0xF3;
  cmd[2] = 0xAB;
  cmd[3] = (uint8_t)state;
  cmd[4] = (uint8_t)snF;
  cmd[5] = 0x6B;
  usart_SendCmd(cmd, sizeof(cmd));
}

void X_V5_Vel_Control(uint8_t addr, uint8_t dir, uint16_t acc, uint16_t speed, bool snF)
{
  /* X 固件速度模式控制（5.3.5）：Addr F6 dir acc(2B) speed(2B) sync 6B fileciteturn2file1L11-L18 */
  uint8_t cmd[9] = {0};
  cmd[0] = addr;
  cmd[1] = 0xF6;
  cmd[2] = dir;
  cmd[3] = (uint8_t)(acc >> 8);
  cmd[4] = (uint8_t)(acc >> 0);
  cmd[5] = (uint8_t)(speed >> 8);
  cmd[6] = (uint8_t)(speed >> 0);
  cmd[7] = (uint8_t)snF;
  cmd[8] = 0x6B;
  usart_SendCmd(cmd, 9);
}

void X_V5_Stop_Now(uint8_t addr, bool snF)
{
  uint8_t cmd[5] = {0};
  cmd[0] = addr;
  cmd[1] = 0xFE;
  cmd[2] = 0x98;
  cmd[3] = (uint8_t)snF;
  cmd[4] = 0x6B;
  usart_SendCmd(cmd, sizeof(cmd));
}

void X_V5_Synchronous_motion(uint8_t addr)
{
  /* 触发多机同步运动命令：广播 00 FF 66 6B，返回地址+FF+02+6B（手册示例）fileciteturn2file14L19-L20 */
  uint8_t cmd[4] = { addr, 0xFF, 0x66, 0x6B };
  usart_SendCmd(cmd, 4);
}

void X_V5_Origin_Set_O(uint8_t addr, bool svF)
{
  uint8_t cmd[5] = { addr, 0x93, 0x88, (uint8_t)svF, 0x6B };
  usart_SendCmd(cmd, sizeof(cmd));
}

void X_V5_Origin_Modify_Params(uint8_t addr, bool svF, uint8_t o_mode, uint8_t o_dir, uint16_t o_vel, uint32_t o_tm,
                               uint16_t sl_vel, uint16_t sl_ma, uint16_t sl_ms, bool potF)
{
  /* 与原工程 Emm_V5_Origin_Modify_Params 采用一致封装：本工程侧只负责“发帧”，
     若你后续要严格按 X 固件差异调整回零参数字节定义，请告诉我你的目标回零模式。 */
  uint8_t cmd[32] = {0};
  uint8_t i = 0;

  cmd[i++] = addr;
  cmd[i++] = 0x4C;
  cmd[i++] = 0xC4;
  cmd[i++] = (uint8_t)svF;
  cmd[i++] = o_mode;
  cmd[i++] = o_dir;
  cmd[i++] = (uint8_t)(o_vel >> 8);
  cmd[i++] = (uint8_t)(o_vel >> 0);
  cmd[i++] = (uint8_t)(o_tm >> 24);
  cmd[i++] = (uint8_t)(o_tm >> 16);
  cmd[i++] = (uint8_t)(o_tm >> 8);
  cmd[i++] = (uint8_t)(o_tm >> 0);
  cmd[i++] = (uint8_t)(sl_vel >> 8);
  cmd[i++] = (uint8_t)(sl_vel >> 0);
  cmd[i++] = (uint8_t)(sl_ma >> 8);
  cmd[i++] = (uint8_t)(sl_ma >> 0);
  cmd[i++] = (uint8_t)(sl_ms >> 8);
  cmd[i++] = (uint8_t)(sl_ms >> 0);
  cmd[i++] = (uint8_t)potF;
  cmd[i++] = 0x6B;

  usart_SendCmd(cmd, i);
}

void X_V5_Origin_Trigger_Return(uint8_t addr, uint8_t o_mode, bool snF)
{
  uint8_t cmd[6] = {0};
  cmd[0] = addr;
  cmd[1] = 0x9A;
  cmd[2] = 0x65;
  cmd[3] = o_mode;
  cmd[4] = (uint8_t)snF;
  cmd[5] = 0x6B;
  usart_SendCmd(cmd, sizeof(cmd));
}

void X_V5_Origin_Interrupt(uint8_t addr)
{
  uint8_t cmd[4] = { addr, 0x9C, 0x48, 0x6B };
  usart_SendCmd(cmd, sizeof(cmd));
}
