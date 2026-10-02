#include "modbus_tcp_server.h"
#include <string.h>

static uint16_t s_input_registers[MODBUS_INPUT_REGISTER_COUNT];
static uint8_t s_coils[MODBUS_COIL_COUNT];
static uint8_t s_command_seen;
static uint32_t s_last_command_ms;
static ModbusTcp_CoilWriteCallback s_coil_callback;

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void write_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void apply_coil(uint8_t index, uint8_t value)
{
    value = value ? 1U : 0U;
    if (index >= MODBUS_COIL_COUNT) return;
    s_coils[index] = value;
    if (s_coil_callback != 0) s_coil_callback(index, value);
}

static uint16_t exception_response(const uint8_t *request, uint8_t function,
                                   uint8_t exception, uint8_t *response,
                                   uint16_t capacity)
{
    if (capacity < 9U) return 0U;
    response[0] = request[0];
    response[1] = request[1];
    response[2] = 0U;
    response[3] = 0U;
    response[4] = 0U;
    response[5] = 3U;
    response[6] = MODBUS_TCP_UNIT_ID;
    response[7] = (uint8_t)(function | 0x80U);
    response[8] = exception;
    return 9U;
}

void ModbusTcp_Init(ModbusTcp_CoilWriteCallback callback)
{
    memset(s_input_registers, 0, sizeof(s_input_registers));
    memset(s_coils, 0, sizeof(s_coils));
    s_command_seen = 0U;
    s_last_command_ms = 0U;
    s_coil_callback = callback;
}

void ModbusTcp_SetInputRegisters(const uint16_t *registers, uint16_t count)
{
    if (registers == 0) return;
    if (count > MODBUS_INPUT_REGISTER_COUNT) count = MODBUS_INPUT_REGISTER_COUNT;
    memcpy(s_input_registers, registers, (uint32_t)count * sizeof(uint16_t));
}

uint16_t ModbusTcp_ProcessAdu(const uint8_t *request, uint16_t request_len,
                             uint8_t *response, uint16_t response_capacity,
                             uint32_t now_ms)
{
    uint16_t mbap_len;
    uint8_t function;
    uint16_t address;
    uint16_t quantity;
    uint16_t i;

    if (request == 0 || response == 0 || request_len < 8U) return 0U;
    if (request[2] != 0U || request[3] != 0U) return 0U;
    mbap_len = read_be16(request + 4);
    if (mbap_len < 2U || (uint32_t)mbap_len + 6U != request_len) return 0U;
    if (request[6] != MODBUS_TCP_UNIT_ID) return 0U;
    function = request[7];

    if (function == 0x04U) {
        uint16_t data_bytes;
        uint16_t response_len;
        if (request_len != 12U) return exception_response(request, function, 0x03U, response, response_capacity);
        address = read_be16(request + 8);
        quantity = read_be16(request + 10);
        if (quantity == 0U || quantity > 125U) return exception_response(request, function, 0x03U, response, response_capacity);
        if (address >= MODBUS_INPUT_REGISTER_COUNT ||
            (uint32_t)address + quantity > MODBUS_INPUT_REGISTER_COUNT)
            return exception_response(request, function, 0x02U, response, response_capacity);
        data_bytes = (uint16_t)(quantity * 2U);
        response_len = (uint16_t)(9U + data_bytes);
        if (response_capacity < response_len) return 0U;
        memcpy(response, request, 2U);
        response[2] = response[3] = response[4] = 0U;
        response[5] = (uint8_t)(3U + data_bytes);
        response[6] = MODBUS_TCP_UNIT_ID;
        response[7] = function;
        response[8] = (uint8_t)data_bytes;
        for (i = 0U; i < quantity; ++i) write_be16(response + 9U + i * 2U, s_input_registers[address + i]);
        return response_len;
    }

    if (function == 0x01U) {
        uint16_t byte_count;
        uint16_t response_len;
        if (request_len != 12U) return exception_response(request, function, 0x03U, response, response_capacity);
        address = read_be16(request + 8);
        quantity = read_be16(request + 10);
        if (quantity == 0U || quantity > 2000U) return exception_response(request, function, 0x03U, response, response_capacity);
        if (address >= MODBUS_COIL_COUNT || (uint32_t)address + quantity > MODBUS_COIL_COUNT)
            return exception_response(request, function, 0x02U, response, response_capacity);
        byte_count = (uint16_t)((quantity + 7U) / 8U);
        response_len = (uint16_t)(9U + byte_count);
        if (response_capacity < response_len) return 0U;
        memcpy(response, request, 2U);
        response[2] = response[3] = response[4] = 0U;
        response[5] = (uint8_t)(3U + byte_count);
        response[6] = MODBUS_TCP_UNIT_ID;
        response[7] = function;
        response[8] = (uint8_t)byte_count;
        memset(response + 9, 0, byte_count);
        for (i = 0U; i < quantity; ++i) if (s_coils[address + i]) response[9U + i / 8U] |= (uint8_t)(1U << (i % 8U));
        return response_len;
    }

    if (function == 0x05U) {
        uint16_t value;
        if (request_len != 12U) return exception_response(request, function, 0x03U, response, response_capacity);
        address = read_be16(request + 8);
        value = read_be16(request + 10);
        if (address >= MODBUS_COIL_COUNT) return exception_response(request, function, 0x02U, response, response_capacity);
        if (value != 0xFF00U && value != 0x0000U) return exception_response(request, function, 0x03U, response, response_capacity);
        if (response_capacity < 12U) return 0U;
        apply_coil((uint8_t)address, value == 0xFF00U);
        s_command_seen = 1U;
        s_last_command_ms = now_ms;
        memcpy(response, request, 12U);
        return 12U;
    }

    if (function == 0x0FU) {
        uint8_t byte_count;
        if (request_len < 14U) return exception_response(request, function, 0x03U, response, response_capacity);
        address = read_be16(request + 8);
        quantity = read_be16(request + 10);
        byte_count = request[12];
        if (quantity == 0U || quantity > 1968U || byte_count != (uint8_t)((quantity + 7U) / 8U) ||
            request_len != (uint16_t)(13U + byte_count))
            return exception_response(request, function, 0x03U, response, response_capacity);
        if (address >= MODBUS_COIL_COUNT || (uint32_t)address + quantity > MODBUS_COIL_COUNT)
            return exception_response(request, function, 0x02U, response, response_capacity);
        if (response_capacity < 12U) return 0U;
        for (i = 0U; i < quantity; ++i) apply_coil((uint8_t)(address + i), (request[13U + i / 8U] >> (i % 8U)) & 1U);
        s_command_seen = 1U;
        s_last_command_ms = now_ms;
        memcpy(response, request, 8U);
        response[4] = 0U;
        response[5] = 6U;
        memcpy(response + 8, request + 8, 4U);
        return 12U;
    }

    return exception_response(request, function, 0x01U, response, response_capacity);
}

void ModbusTcp_GetCoils(uint8_t coils[MODBUS_COIL_COUNT])
{
    if (coils != 0) memcpy(coils, s_coils, MODBUS_COIL_COUNT);
}

uint8_t ModbusTcp_CommandIsFresh(uint32_t now_ms, uint32_t timeout_ms)
{
    return (uint8_t)(s_command_seen && ((uint32_t)(now_ms - s_last_command_ms) <= timeout_ms));
}

void ModbusTcp_ApplySafetyTimeout(uint32_t now_ms, uint32_t timeout_ms)
{
    uint8_t i;
    if (!s_command_seen || (uint32_t)(now_ms - s_last_command_ms) <= timeout_ms) return;
    for (i = 0U; i < MODBUS_COIL_COUNT; ++i) apply_coil(i, 0U);
    s_command_seen = 0U;
}
