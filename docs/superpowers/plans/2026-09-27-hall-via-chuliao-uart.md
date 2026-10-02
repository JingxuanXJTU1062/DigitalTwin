# F407HALL 经出料板串口汇聚 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 F407HALL 的原16字节数据帧经 USART1送到F103chuliao05，再由出料板把原12字节出料帧与最新有效HALL帧连续发送给上位机UART5。

**Architecture:** F407HALL只负责生成并发送现有 `0x03/16B` 帧；F103chuliao05新增独立的HALL字节流解析器和接收快照，并在自己的发送周期构建12或28字节稳定发送快照；上位机移除I2C3轮询，沿用UART5通用状态机解析连续的 `0x05` 与 `0x03` 帧。三个组件通过原帧接口解耦，不引入新的合并帧格式。

**Tech Stack:** STM32 HAL C、STM32F407、STM32F103、Keil MDK-ARM、CMake/STM32Cube工具链、PowerShell源代码契约测试。

**Spec:** `docs/superpowers/specs/2026-09-27-hall-via-chuliao-uart-design.md`

## Global Constraints

- 两段串口均固定为115200、8-N-1、无硬件流控。
- F407HALL帧保持16字节、模块ID `0x03`、载荷长度 `0x0B`、小端字段和原XOR算法。
- F103chuliao05帧保持12字节、模块ID `0x05`、载荷长度 `0x07`和原XOR算法。
- 有效HALL快照新鲜期固定为500 ms；过期时只发送12字节出料帧。
- F407HALL03、其他三块下位机、Modbus寄存器布局和PC脚本格式不得改变。
- 当前目录不是Git仓库；计划中的每个“检查点”保存构建与测试证据，不能执行Git提交。

## Review Focus

- HALL字节流在任意字节边界启动：解析器必须重新同步到 `AA 55`，对应Task 2分段/噪声测试。
- 接收到错误XOR或错误长度：不得覆盖上一份有效快照，对应Task 2无效帧测试。
- USART1正在向上位机发送时收到HALL字节：RX中断必须继续工作且发送缓冲不得被改写，对应Task 2快照隔离测试。
- HALL恰在500 ms边界：`age <= 500`发送28字节，`age > 500`发送12字节，对应Task 2边界测试。
- 上位机一次收到两个无间隔帧：必须分别更新C路和D路，对应Task 3连续帧测试。

---

### Task 1: F407HALL改为USART1发送原16字节帧

**Files:**
- Modify: `xiaweiji/F407HALL/Core/Inc/distance_tx.h`
- Modify: `xiaweiji/F407HALL/Core/Src/distance_tx.c`
- Modify: `xiaweiji/F407HALL/Core/Src/main.c`
- Modify: `xiaweiji/F407HALL/CMakeLists.txt`（仅当测试辅助源需要加入）
- Test: `xiaweiji/F407HALL/tests/check_uart_transport.ps1`

**Interfaces:**
- Consumes: `UART_HandleTypeDef huart1`，现有 `DistanceTx_Send(uint32_t,uint32_t,uint32_t,uint16_t,uint16_t,uint8_t)`参数。
- Produces: `void DistanceTx_Init(UART_HandleTypeDef *uart)`；每次成功调用在USART1发送原16字节帧；诊断量 `g_uart_tx_count`、`g_uart_tx_error_count`、`g_uart_last_frame[16]`。

- [ ] **Step 1: 写失败的源代码契约测试**

在 `check_uart_transport.ps1` 断言：`DistanceTx_Init`参数为 `UART_HandleTypeDef *`；实现调用 `HAL_UART_Transmit`且长度为16；`main.c`使用 `DistanceTx_Init(&huart1)`；板间通信路径不再调用 `MX_I2C3_Init()`或 `HAL_I2C_Slave_Transmit_IT()`。

- [ ] **Step 2: 运行测试确认因当前I2C实现而失败**

Run: `powershell -ExecutionPolicy Bypass -File xiaweiji/F407HALL/tests/check_uart_transport.ps1`

Expected: FAIL，明确指出仍使用I2C句柄/从机发送。

- [ ] **Step 3: 最小实现UART发送传输层**

将 `DistanceTx_Init`改为保存UART句柄；保留现有16字节组帧代码；用 `HAL_UART_Transmit(..., 16U, 20U)`发送；成功/失败分别更新计数和最后状态。移除I2C回调与重挂逻辑。

- [ ] **Step 4: 修改F407HALL初始化和主循环接线点**

在 `main.c`保留 `MX_USART1_UART_Init()`，改为 `DistanceTx_Init(&huart1)`，移除板间通信所需的 `MX_I2C3_Init()`；不得改动I2C1/I2C2传感器初始化。

- [ ] **Step 5: 运行契约测试并构建F407HALL**

Run: `powershell -ExecutionPolicy Bypass -File xiaweiji/F407HALL/tests/check_uart_transport.ps1`

Expected: PASS。

Run: `cmake --build xiaweiji/F407HALL/build/Debug`

Expected: `F407HALL.elf`链接成功、0 errors。

- [ ] **Step 6: 保存检查点证据**

记录测试输出、ELF时间戳和内存占用；当前工作区无Git仓库，不执行commit。

### Task 2: F103chuliao05接收HALL并构建12/28字节发送批次

**Files:**
- Create: `xiaweiji/F103chuliao05/Core/Inc/hall_bridge.h`
- Create: `xiaweiji/F103chuliao05/Core/Src/hall_bridge.c`
- Modify: `xiaweiji/F103chuliao05/Core/Inc/usart_protocol.h`
- Modify: `xiaweiji/F103chuliao05/Core/Src/usart_protocol.c`
- Modify: `xiaweiji/F103chuliao05/Core/Src/main.c`
- Modify: `xiaweiji/F103chuliao05/MDK-ARM/F103chuliao05.uvprojx`
- Test: `tests/test_hall_bridge_contract.ps1`

**Interfaces:**
- Consumes: F407HALL逐字节USART1 RX流；现有12字节 `g_tx_frame`出料数据；`HAL_GetTick()`。
- Produces: `void HallBridge_Init(UART_HandleTypeDef *uart)`、`void HallBridge_PushByte(uint8_t byte, uint32_t now_ms)`、`uint8_t HallBridge_CopyFreshFrame(uint8_t out[16], uint32_t now_ms, uint32_t timeout_ms)`；诊断量 `g_hall_bridge_rx_count`、`g_hall_bridge_xor_error_count`、`g_hall_bridge_last_frame[16]`、`g_hall_bridge_valid`、`g_hall_bridge_timestamp`。

- [ ] **Step 1: 写失败的桥接契约测试**

测试断言新接口和诊断量存在；接收状态机固定接受 `AA 55 03 0B`和16字节总长；XOR范围为索引2到14；新鲜判断严格为 `now_ms - timestamp <= 500U`；最大发送快照为28字节且顺序为12字节出料帧后16字节HALL帧。

- [ ] **Step 2: 运行测试确认接口尚不存在**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_hall_bridge_contract.ps1`

Expected: FAIL，指出 `hall_bridge.h/.c`或28字节快照缺失。

- [ ] **Step 3: 实现独立HALL字节流解析器**

在 `hall_bridge.h/.c`实现上述三个接口。状态机支持帧头重同步、分段字节流；只有帧头、ID、长度和XOR全部正确才以临界区方式更新16字节接收快照及时间戳；错误帧只增加错误计数。

- [ ] **Step 4: 接入USART1单字节中断接收**

在 `Protocol_Init()`后调用 `HallBridge_Init(&huart1)`并启动 `HAL_UART_Receive_IT(..., 1U)`；实现 `HAL_UART_RxCpltCallback`将字节交给 `HallBridge_PushByte`并立即重新挂接接收；实现 `HAL_UART_ErrorCallback`清理ORE/FE/NE并重新挂接。现有TX完成回调保持有效。

- [ ] **Step 5: 将出料发送快照扩展为12或28字节**

保持原12字节构建函数不变；新增稳定的 `g_tx_batch[28]`和 `g_tx_batch_len`。每次发送先复制12字节出料帧，再调用 `HallBridge_CopyFreshFrame(..., HAL_GetTick(), 500U)`；返回1时追加16字节并发送28字节，否则发送12字节。异步发送期间不得写 `g_tx_batch`。

- [ ] **Step 6: 补足Review Focus契约断言**

在测试中加入噪声前缀/分段输入、错误XOR不覆盖、500/501 ms边界、TX缓冲与RX快照分离的断言或结构检查。

- [ ] **Step 7: 运行测试并构建F103chuliao05**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_hall_bridge_contract.ps1`

Expected: PASS。

Run: `D:\Keil_v5\UV4\UV4.exe -b xiaweiji/F103chuliao05/MDK-ARM/F103chuliao05.uvprojx -j0 -o xiaweiji/F103chuliao05/MDK-ARM/hall_bridge_build.log`

Expected: `0 Error(s), 0 Warning(s)`，生成新的HEX/AXF。

- [ ] **Step 8: 保存检查点证据**

记录契约测试、Keil日志和产物时间戳；当前工作区无Git仓库，不执行commit。

### Task 3: 上位机改为仅从UART5统一解析出料与HALL

**Files:**
- Modify: `WiFi/F407_04/User/main.c`
- Modify: `WiFi/F407_04/Project/Fire_F407ZG.uvprojx`
- Modify: `WiFi/F407_04/User/usart_protocol/sensor_frame.c`
- Modify: `WiFi/F407_04/User/usart_protocol/sensor_frame.h`
- Test: `WiFi/F407_04/tests/test_uart5_aggregate_contract.ps1`
- Test: `WiFi/F407_04/tests/test_protocol_freshness.c`

**Interfaces:**
- Consumes: UART5字节流中连续的12字节 `0x05`帧和16字节 `0x03`帧。
- Produces: 现有 `g_chuliao_data`、`g_rx_last_frame_hall_vl6180x`、`g_rx_count_chuliao`、`g_rx_count_hall_vl6180x`和不变的30个Modbus输入寄存器。

- [ ] **Step 1: 写失败的上位机链路契约测试**

断言 `main.c`不再包含 `I2C3_HALL_Config()`或 `SensorFrame_PollI2C3()`；UART5仍调用 `Protocol_ParseByte(UART_IDX_UART5_HALL, byte)`；状态机接受同一UART索引上的ID `0x05/12B`和 `0x03/16B`；Modbus有效位仍为C=`0x02`、D=`0x08`。

- [ ] **Step 2: 运行测试确认当前仍依赖I2C3**

Run: `powershell -ExecutionPolicy Bypass -File WiFi/F407_04/tests/test_uart5_aggregate_contract.ps1`

Expected: FAIL，指出I2C3初始化/轮询仍存在。

- [ ] **Step 3: 移除运行时I2C3 HALL路径**

从 `main.c`移除 `bsp_i2c3_hall.h`、`I2C3_HALL_Config()`和快照周期中的 `SensorFrame_PollI2C3()`；保留UART5轮询、中断和通用协议处理。将 `sensor_frame`改为仅保留兼容初始化接口或从工程中移除其I2C轮询职责，并从Keil工程移除不再使用的 `bsp_i2c3_hall.c`。

- [ ] **Step 4: 添加连续双帧解析验证**

扩展协议测试：依次向 `Protocol_ParseByte(UART_IDX_UART5_HALL, ...)`输入有效12字节 `0x05`帧与16字节 `0x03`帧且中间无间隔，断言两个接收计数各增加1、两路valid均为1；推进时间超过500 ms后断言D路失效。

- [ ] **Step 5: 运行测试并构建上位机**

Run: `powershell -ExecutionPolicy Bypass -File WiFi/F407_04/tests/test_uart5_aggregate_contract.ps1`

Expected: PASS。

Run: `D:\Keil_v5\UV4\UV4.exe -b WiFi/F407_04/Project/Fire_F407ZG.uvprojx -j0 -o WiFi/F407_04/Output/hall_uart_aggregate_build.log`

Expected: `0 Error(s), 0 Warning(s)`并生成新HEX。

- [ ] **Step 6: 保存检查点证据**

记录测试、构建日志和固件时间戳；当前工作区无Git仓库，不执行commit。

### Task 4: 文档、全链路静态校验与交付

**Files:**
- Modify: `上位机与5个下位机引脚清单.md`
- Test: `tests/test_hall_uart_wiring_contract.ps1`

**Interfaces:**
- Consumes: Tasks 1–3确定的串口、帧和超时接口。
- Produces: 唯一接线清单、三份可烧录固件和板上验证步骤。

- [ ] **Step 1: 写失败的接线一致性测试**

断言清单包含 `F407HALL PA9 → F103chuliao05 PA10`、`F103chuliao05 PA9 → 上位机 PD2`和三板共地；不得再把F407HALL PA8/PC9列为上位机通信线。

- [ ] **Step 2: 运行测试确认旧I2C接线仍存在**

Run: `powershell -ExecutionPolicy Bypass -File tests/test_hall_uart_wiring_contract.ps1`

Expected: FAIL。

- [ ] **Step 3: 更新引脚清单和Watch变量**

写明两段115200 8-N-1单向串口接线、28字节批次实际由两个原帧组成、500 ms失效规则，以及三端发送/接收计数变量。

- [ ] **Step 4: 运行全部契约测试**

Run: `powershell -ExecutionPolicy Bypass -File xiaweiji/F407HALL/tests/check_uart_transport.ps1`

Run: `powershell -ExecutionPolicy Bypass -File tests/test_hall_bridge_contract.ps1`

Run: `powershell -ExecutionPolicy Bypass -File WiFi/F407_04/tests/test_uart5_aggregate_contract.ps1`

Run: `powershell -ExecutionPolicy Bypass -File tests/test_hall_uart_wiring_contract.ps1`

Expected: 全部PASS。

- [ ] **Step 5: 从干净输入重新构建三个固件并核对产物**

按Tasks 1–3命令重新构建；逐个读取日志，确认全部为0 errors，并核对ELF/HEX/AXF时间戳晚于对应源码。

- [ ] **Step 6: 执行板上验收**

烧录三份新固件后按规格接线。确认F407HALL发送计数增加、出料板HALL接收计数和28字节发送长度正常、上位机 `g_uart5_hall_irq_count`与 `g_rx_count_hall_vl6180x`增加、PC端显示 `C1 D1`；断开第一段串口超过500 ms后确认 `C1 D0`。
