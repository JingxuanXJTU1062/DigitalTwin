#ifndef VL6180X_H
#define VL6180X_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define VL6180X_DEFAULT_I2C_ADDRESS  0x29U
#define VL6180X_EXPECTED_MODEL_ID    0xB4U

typedef enum
{
  VL6180X_STATUS_OK = 0,
  VL6180X_STATUS_INVALID_ARGUMENT,
  VL6180X_STATUS_I2C_ERROR,
  VL6180X_STATUS_WRONG_DEVICE,
  VL6180X_STATUS_TIMEOUT,
  VL6180X_STATUS_RANGE_ERROR
} VL6180X_StatusTypeDef;

typedef enum
{
  VL6180X_MEASURE_STAGE_IDLE = 0,
  VL6180X_MEASURE_STAGE_WAIT_READY,
  VL6180X_MEASURE_STAGE_START,
  VL6180X_MEASURE_STAGE_WAIT_COMPLETE,
  VL6180X_MEASURE_STAGE_READ_STATUS,
  VL6180X_MEASURE_STAGE_READ_DISTANCE,
  VL6180X_MEASURE_STAGE_CLEAR_INTERRUPT,
  VL6180X_MEASURE_STAGE_DONE
} VL6180X_MeasureStageTypeDef;

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint16_t i2c_address;
  uint32_t timeout_ms;
  uint8_t model_id;
  uint8_t range_status;
  uint8_t initialized;
  uint8_t measure_stage;
} VL6180X_HandleTypeDef;

VL6180X_StatusTypeDef VL6180X_Init(VL6180X_HandleTypeDef *device,
                                   I2C_HandleTypeDef *i2c);
VL6180X_StatusTypeDef VL6180X_SetI2CAddress(VL6180X_HandleTypeDef *device,
                                            uint8_t new_7bit_address);
VL6180X_StatusTypeDef VL6180X_ReadModelId(VL6180X_HandleTypeDef *device,
                                          uint8_t *model_id);
VL6180X_StatusTypeDef VL6180X_ReadDistance(VL6180X_HandleTypeDef *device,
                                           uint8_t *distance_mm);

#ifdef __cplusplus
}
#endif

#endif /* VL6180X_H */
