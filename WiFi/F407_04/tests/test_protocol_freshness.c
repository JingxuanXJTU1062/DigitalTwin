#include <assert.h>
#include <stdint.h>
#include "usart_protocol.h"

int main(void)
{
    uint16_t regs[30] = {0};
    Protocol_ExpireStaleData(1000U, 500U);
    Protocol_BuildModbusInputRegisters(regs, 30, 7, 0);
    assert(regs[0] == 0x0100U);
    assert(regs[1] == 7U);
    return 0;
}
