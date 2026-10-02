#ifndef HALL03_FRAME_H
#define HALL03_FRAME_H

#include <stdint.h>

#define HALL03_FRAME_SIZE 17U
#define HALL03_MODULE_ID  0x04U
#define HALL03_PAYLOAD_LEN 0x0CU

void Hall03Frame_Build(uint8_t frame[HALL03_FRAME_SIZE],
                       uint32_t omega1_mrad, uint32_t omega2_mrad,
                       uint16_t dist1_mm, uint16_t dist2_mm,
                       uint16_t dist3_mm, uint16_t range_status);

#endif
