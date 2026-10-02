#include <assert.h>
#include <stdint.h>
#include "hall03_frame.h"

int main(void)
{
    uint8_t frame[HALL03_FRAME_SIZE];
    static const uint8_t expected[HALL03_FRAME_SIZE] = {
        0xAA, 0x55, 0x04, 0x0C,
        0x34, 0x12, 0xCD, 0xAB,
        0x11, 0x00, 0x22, 0x00, 0x33, 0x00,
        0x21, 0x03, 0x6A
    };

    Hall03Frame_Build(frame, 0x1234U, 0xABCDU,
                      0x0011U, 0x0022U, 0x0033U, 0x0321U);
    for (uint32_t i = 0U; i < HALL03_FRAME_SIZE; ++i) {
        assert(frame[i] == expected[i]);
    }

    Hall03Frame_Build(frame, 70000U, 80000U, 1U, 2U, 3U, 0U);
    assert(frame[4] == 0xFFU && frame[5] == 0xFFU);
    assert(frame[6] == 0xFFU && frame[7] == 0xFFU);
    return 0;
}
