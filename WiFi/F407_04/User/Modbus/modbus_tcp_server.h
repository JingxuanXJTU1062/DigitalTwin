#ifndef MODBUS_TCP_SERVER_H
#define MODBUS_TCP_SERVER_H

#include <stdint.h>

#define MODBUS_TCP_UNIT_ID               1U
#define MODBUS_INPUT_REGISTER_COUNT      30U
#define MODBUS_COIL_COUNT                 4U
#define MODBUS_TCP_MAX_ADU_SIZE         260U

typedef void (*ModbusTcp_CoilWriteCallback)(uint8_t index, uint8_t value);

void ModbusTcp_Init(ModbusTcp_CoilWriteCallback callback);
void ModbusTcp_SetInputRegisters(const uint16_t *registers, uint16_t count);
uint16_t ModbusTcp_ProcessAdu(const uint8_t *request, uint16_t request_len,
                             uint8_t *response, uint16_t response_capacity,
                             uint32_t now_ms);
void ModbusTcp_GetCoils(uint8_t coils[MODBUS_COIL_COUNT]);
uint8_t ModbusTcp_CommandIsFresh(uint32_t now_ms, uint32_t timeout_ms);
void ModbusTcp_ApplySafetyTimeout(uint32_t now_ms, uint32_t timeout_ms);

#endif
