#ifndef __VL53_APP_H
#define __VL53_APP_H

#include <stdint.h>
#include <stdbool.h>

// 初始化 VL53（包含 I2C ping + 全部配置）
bool VL53_App_Init(void);

// 周期调用：更新一次测距（非阻塞）
// 返回 true 表示本次成功拿到“新数据”
bool VL53_App_Update(void);

// 查询接口（只读）
bool     VL53_App_IsDataValid(void);
uint16_t VL53_App_GetRawMM(void);
uint16_t VL53_App_GetFiltMM(void);

// 无数据/异常时调用：内部会 I2C recover + Stop/Start（必要时重初始化）
void VL53_App_RestartRanging(void);

#endif
