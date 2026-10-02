#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "modbus_tcp_server.h"

static void test_read_input_registers(void)
{
    uint16_t regs[MODBUS_INPUT_REGISTER_COUNT] = {0};
    uint8_t request[] = {0x12,0x34,0,0,0,6,1,4,0,10,0,2};
    uint8_t response[64] = {0};
    uint16_t n;
    regs[10] = 0x1234;
    regs[11] = 1;
    ModbusTcp_Init(0);
    ModbusTcp_SetInputRegisters(regs, MODBUS_INPUT_REGISTER_COUNT);
    n = ModbusTcp_ProcessAdu(request, sizeof(request), response, sizeof(response), 0);
    assert(n == 13);
    assert(memcmp(response, (uint8_t[]){0x12,0x34,0,0,0,7,1,4,4,0x12,0x34,0,1}, 13) == 0);
}

static uint8_t callback_index;
static uint8_t callback_value;

static void record_coil(uint8_t index, uint8_t value)
{
    callback_index = index;
    callback_value = value;
}

static void test_write_and_read_fourth_coil(void)
{
    uint8_t write_request[] = {0x00,0x01,0,0,0,6,1,5,0,3,0xFF,0};
    uint8_t read_request[] = {0x00,0x02,0,0,0,6,1,1,0,0,0,4};
    uint8_t response[64] = {0};
    uint16_t n;

    ModbusTcp_Init(record_coil);
    n = ModbusTcp_ProcessAdu(write_request, sizeof(write_request), response,
                             sizeof(response), 10);
    assert(n == 12);
    assert(callback_index == 3);
    assert(callback_value == 1);

    n = ModbusTcp_ProcessAdu(read_request, sizeof(read_request), response,
                             sizeof(response), 11);
    assert(n == 10);
    assert(response[8] == 1);
    assert((response[9] & 0x0F) == 0x08);
}

int main(void)
{
    test_read_input_registers();
    test_write_and_read_fourth_coil();
    return 0;
}
