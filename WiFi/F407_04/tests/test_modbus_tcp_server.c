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

int main(void)
{
    test_read_input_registers();
    return 0;
}
