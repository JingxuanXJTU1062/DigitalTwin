#include "hall03_frame.h"

void Hall03Frame_Build(uint8_t frame[HALL03_FRAME_SIZE],
                       uint32_t omega1_mrad, uint32_t omega2_mrad,
                       uint16_t dist1_mm, uint16_t dist2_mm,
                       uint16_t dist3_mm, uint16_t range_status)
{
  uint8_t checksum = 0U;
  if (omega1_mrad > 65534U) omega1_mrad = 0U;
  if (omega2_mrad > 65534U) omega2_mrad = 0U;

  frame[0] = 0xAAU;
  frame[1] = 0x55U;
  frame[2] = HALL03_MODULE_ID;
  frame[3] = HALL03_PAYLOAD_LEN;
  frame[4] = (uint8_t)omega1_mrad;
  frame[5] = (uint8_t)(omega1_mrad >> 8U);
  frame[6] = (uint8_t)omega2_mrad;
  frame[7] = (uint8_t)(omega2_mrad >> 8U);
  frame[8] = (uint8_t)dist1_mm;
  frame[9] = (uint8_t)(dist1_mm >> 8U);
  frame[10] = (uint8_t)dist2_mm;
  frame[11] = (uint8_t)(dist2_mm >> 8U);
  frame[12] = (uint8_t)dist3_mm;
  frame[13] = (uint8_t)(dist3_mm >> 8U);
  frame[14] = (uint8_t)range_status;
  frame[15] = (uint8_t)(range_status >> 8U);
  for (uint32_t i = 2U; i <= 15U; ++i) checksum ^= frame[i];
  frame[16] = checksum;
}
