/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    vl6180x.c
  * @brief   VL6180X ToF distance sensor driver
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "vl6180x.h"

#define VL6180X_REG_MODEL_ID                 0x0000U
#define VL6180X_REG_INTERRUPT_CONFIG_GPIO    0x0014U
#define VL6180X_REG_INTERRUPT_CLEAR          0x0015U
#define VL6180X_REG_FRESH_OUT_OF_RESET        0x0016U
#define VL6180X_REG_SYSRANGE_START           0x0018U
#define VL6180X_REG_RESULT_RANGE_STATUS       0x004DU
#define VL6180X_REG_RESULT_INTERRUPT_STATUS  0x004FU
#define VL6180X_REG_RESULT_RANGE_VALUE       0x0062U

#define VL6180X_DEFAULT_TIMEOUT_MS           100U
#define VL6180X_RANGE_READY_MASK             0x01U
#define VL6180X_RANGE_COMPLETE_MASK          0x07U
#define VL6180X_RANGE_COMPLETE_VALUE         0x04U
#define VL6180X_RANGE_ERROR_MASK             0xF0U

typedef struct
{
  uint16_t address;
  uint8_t value;
} VL6180X_RegisterValue;

/*
 * ST's mandatory private-register tuning sequence.  These registers are
 * intentionally kept private to the driver because the application should
 * not depend on their undocumented meanings.
 */
static const VL6180X_RegisterValue vl6180x_private_settings[] =
{
  {0x0207U, 0x01U}, {0x0208U, 0x01U}, {0x0133U, 0x01U},
  {0x0096U, 0x00U}, {0x0097U, 0xFDU}, {0x00E3U, 0x00U},
  {0x00E4U, 0x04U}, {0x00E5U, 0x02U}, {0x00E6U, 0x01U},
  {0x00E7U, 0x03U}, {0x00F5U, 0x02U}, {0x00D9U, 0x05U},
  {0x00DBU, 0xCEU}, {0x00DCU, 0x03U}, {0x00DDU, 0xF8U},
  {0x009FU, 0x00U}, {0x00A3U, 0x3CU}, {0x00B7U, 0x00U},
  {0x00BBU, 0x3CU}, {0x00B2U, 0x09U}, {0x00CAU, 0x09U},
  {0x0198U, 0x01U}, {0x01B0U, 0x17U}, {0x01ADU, 0x00U},
  {0x00FFU, 0x05U}, {0x0100U, 0x05U}, {0x0199U, 0x05U},
  {0x0109U, 0x07U}, {0x010AU, 0x30U}, {0x003FU, 0x46U},
  {0x01A6U, 0x1BU}, {0x01ACU, 0x3EU}, {0x01A7U, 0x1FU},
  {0x0103U, 0x01U}, {0x0030U, 0x00U}
};

/* Public settings recommended by ST for basic single-shot ranging. */
static const VL6180X_RegisterValue vl6180x_public_settings[] =
{
  {0x001BU, 0x0AU},
  {0x003EU, 0x0AU},
  {0x0131U, 0x04U},
  {0x0011U, 0x10U},
  {VL6180X_REG_INTERRUPT_CONFIG_GPIO, 0x24U},
  {0x0031U, 0xFFU},
  {0x00D2U, 0x01U},
  {0x00F2U, 0x01U}
};

static VL6180X_StatusTypeDef VL6180X_ReadByte(VL6180X_HandleTypeDef *device,
                                              uint16_t register_address,
                                              uint8_t *value)
{
  if ((device == NULL) || (device->i2c == NULL) || (value == NULL))
  {
    return VL6180X_STATUS_INVALID_ARGUMENT;
  }

  if (HAL_I2C_Mem_Read(device->i2c,
                       device->i2c_address,
                       register_address,
                       I2C_MEMADD_SIZE_16BIT,
                       value,
                       1U,
                       device->timeout_ms) != HAL_OK)
  {
    return VL6180X_STATUS_I2C_ERROR;
  }

  return VL6180X_STATUS_OK;
}

static VL6180X_StatusTypeDef VL6180X_WriteByte(VL6180X_HandleTypeDef *device,
                                               uint16_t register_address,
                                               uint8_t value)
{
  if ((device == NULL) || (device->i2c == NULL))
  {
    return VL6180X_STATUS_INVALID_ARGUMENT;
  }

  if (HAL_I2C_Mem_Write(device->i2c,
                        device->i2c_address,
                        register_address,
                        I2C_MEMADD_SIZE_16BIT,
                        &value,
                        1U,
                        device->timeout_ms) != HAL_OK)
  {
    return VL6180X_STATUS_I2C_ERROR;
  }

  return VL6180X_STATUS_OK;
}

static VL6180X_StatusTypeDef VL6180X_WriteSettings(
    VL6180X_HandleTypeDef *device,
    const VL6180X_RegisterValue *settings,
    uint32_t setting_count)
{
  uint32_t index;
  VL6180X_StatusTypeDef status;

  for (index = 0U; index < setting_count; index++)
  {
    status = VL6180X_WriteByte(device,
                               settings[index].address,
                               settings[index].value);
    if (status != VL6180X_STATUS_OK)
    {
      return status;
    }
  }

  return VL6180X_STATUS_OK;
}

static VL6180X_StatusTypeDef VL6180X_WaitForBits(
    VL6180X_HandleTypeDef *device,
    uint16_t register_address,
    uint8_t mask,
    uint8_t expected_value)
{
  uint32_t start_tick = HAL_GetTick();
  uint8_t value;
  VL6180X_StatusTypeDef status;

  do
  {
    status = VL6180X_ReadByte(device, register_address, &value);
    if (status != VL6180X_STATUS_OK)
    {
      return status;
    }

    if ((value & mask) == expected_value)
    {
      return VL6180X_STATUS_OK;
    }

    HAL_Delay(1U);
  } while ((HAL_GetTick() - start_tick) < device->timeout_ms);

  return VL6180X_STATUS_TIMEOUT;
}

VL6180X_StatusTypeDef VL6180X_ReadModelId(VL6180X_HandleTypeDef *device,
                                          uint8_t *model_id)
{
  VL6180X_StatusTypeDef status;

  status = VL6180X_ReadByte(device, VL6180X_REG_MODEL_ID, model_id);
  if (status == VL6180X_STATUS_OK)
  {
    device->model_id = *model_id;
  }

  return status;
}

VL6180X_StatusTypeDef VL6180X_Init(VL6180X_HandleTypeDef *device,
                                   I2C_HandleTypeDef *i2c)
{
  VL6180X_StatusTypeDef status;
  uint8_t fresh_out_of_reset;

  if ((device == NULL) || (i2c == NULL))
  {
    return VL6180X_STATUS_INVALID_ARGUMENT;
  }

  device->i2c = i2c;
  device->i2c_address = (uint16_t)(VL6180X_DEFAULT_I2C_ADDRESS << 1U);
  device->timeout_ms = VL6180X_DEFAULT_TIMEOUT_MS;
  device->model_id = 0U;
  device->range_status = 0U;
  device->initialized = 0U;
  device->measure_stage = VL6180X_MEASURE_STAGE_IDLE;

  if (HAL_I2C_IsDeviceReady(device->i2c,
                            device->i2c_address,
                            3U,
                            device->timeout_ms) != HAL_OK)
  {
    return VL6180X_STATUS_I2C_ERROR;
  }

  status = VL6180X_ReadModelId(device, &device->model_id);
  if (status != VL6180X_STATUS_OK)
  {
    return status;
  }

  if (device->model_id != VL6180X_EXPECTED_MODEL_ID)
  {
    return VL6180X_STATUS_WRONG_DEVICE;
  }

  status = VL6180X_ReadByte(device,
                            VL6180X_REG_FRESH_OUT_OF_RESET,
                            &fresh_out_of_reset);
  if (status != VL6180X_STATUS_OK)
  {
    return status;
  }

  if ((fresh_out_of_reset & 0x01U) != 0U)
  {
    status = VL6180X_WriteSettings(
        device,
        vl6180x_private_settings,
        (uint32_t)(sizeof(vl6180x_private_settings) /
                   sizeof(vl6180x_private_settings[0])));
    if (status != VL6180X_STATUS_OK)
    {
      return status;
    }
  }

  status = VL6180X_WriteSettings(
      device,
      vl6180x_public_settings,
      (uint32_t)(sizeof(vl6180x_public_settings) /
                 sizeof(vl6180x_public_settings[0])));
  if (status != VL6180X_STATUS_OK)
  {
    return status;
  }

  status = VL6180X_WriteByte(device,
                             VL6180X_REG_FRESH_OUT_OF_RESET,
                             0x00U);
  if (status != VL6180X_STATUS_OK)
  {
    return status;
  }

  status = VL6180X_WriteByte(device,
                             VL6180X_REG_INTERRUPT_CLEAR,
                             0x07U);
  if (status != VL6180X_STATUS_OK)
  {
    return status;
  }

  device->initialized = 1U;
  return VL6180X_STATUS_OK;
}

/* Sensor 2 is moved off 0x29 before sensor 3 (also 0x29) is released. */
VL6180X_StatusTypeDef VL6180X_SetI2CAddress(VL6180X_HandleTypeDef *device,
                                            uint8_t new_7bit_address)
{
  uint16_t new_address;
  if ((device == NULL) || (device->i2c == NULL) || (new_7bit_address > 0x7FU)) {
    return VL6180X_STATUS_INVALID_ARGUMENT;
  }
  new_address = (uint16_t)(new_7bit_address << 1U);
  if (VL6180X_WriteByte(device, 0x0212U, (uint8_t)new_address) != VL6180X_STATUS_OK) {
    return VL6180X_STATUS_I2C_ERROR;
  }
  device->i2c_address = new_address;
  if (HAL_I2C_IsDeviceReady(device->i2c, device->i2c_address, 3U,
                            device->timeout_ms) != HAL_OK) {
    return VL6180X_STATUS_I2C_ERROR;
  }
  return VL6180X_STATUS_OK;
}

VL6180X_StatusTypeDef VL6180X_ReadDistance(VL6180X_HandleTypeDef *device,
                                           uint8_t *distance_mm)
{
  VL6180X_StatusTypeDef status;
  VL6180X_StatusTypeDef clear_status;
  uint8_t raw_range_status;
  uint8_t measured_distance = 0U;

  if ((device == NULL) || (distance_mm == NULL) ||
      (device->initialized == 0U))
  {
    return VL6180X_STATUS_INVALID_ARGUMENT;
  }

  device->measure_stage = VL6180X_MEASURE_STAGE_WAIT_READY;

  status = VL6180X_WaitForBits(device,
                               VL6180X_REG_RESULT_RANGE_STATUS,
                               VL6180X_RANGE_READY_MASK,
                               VL6180X_RANGE_READY_MASK);
  if (status != VL6180X_STATUS_OK)
  {
    /* Keep the caller's last successful distance unchanged. */
    return status;
  }

  /* SYSRANGE_START is 0x0018.  Writing 0x01 starts one measurement. */
  device->measure_stage = VL6180X_MEASURE_STAGE_START;
  status = VL6180X_WriteByte(device, VL6180X_REG_SYSRANGE_START, 0x01U);
  if (status != VL6180X_STATUS_OK)
  {
    return status;
  }

  device->measure_stage = VL6180X_MEASURE_STAGE_WAIT_COMPLETE;
  status = VL6180X_WaitForBits(device,
                               VL6180X_REG_RESULT_INTERRUPT_STATUS,
                               VL6180X_RANGE_COMPLETE_MASK,
                               VL6180X_RANGE_COMPLETE_VALUE);
  if (status != VL6180X_STATUS_OK)
  {
    /* TIMEOUT on step3 — read what's currently in the result registers anyway */
    uint8_t dbg_raw = 0;
    (void)VL6180X_ReadByte(device, VL6180X_REG_RESULT_RANGE_STATUS, &dbg_raw);
    device->range_status = (uint8_t)(dbg_raw >> 4U);
    (void)VL6180X_ReadByte(device, VL6180X_REG_RESULT_RANGE_VALUE, &measured_distance);
    (void)VL6180X_WriteByte(device, VL6180X_REG_INTERRUPT_CLEAR, 0x07U);
    return status;
  }

  device->measure_stage = VL6180X_MEASURE_STAGE_READ_STATUS;
  status = VL6180X_ReadByte(device,
                            VL6180X_REG_RESULT_RANGE_STATUS,
                            &raw_range_status);
  if (status == VL6180X_STATUS_OK)
  {
    device->range_status = (uint8_t)(raw_range_status >> 4U);
    device->measure_stage = VL6180X_MEASURE_STAGE_READ_DISTANCE;
    status = VL6180X_ReadByte(device,
                              VL6180X_REG_RESULT_RANGE_VALUE,
                              &measured_distance);
  }

  /* 0x0015 is the interrupt-clear register, not the ranging start register. */
  device->measure_stage = VL6180X_MEASURE_STAGE_CLEAR_INTERRUPT;
  clear_status = VL6180X_WriteByte(device,
                                   VL6180X_REG_INTERRUPT_CLEAR,
                                   0x07U);
  if (status != VL6180X_STATUS_OK)
  {
    return status;
  }
  if (clear_status != VL6180X_STATUS_OK)
  {
    return clear_status;
  }

  if ((raw_range_status & VL6180X_RANGE_ERROR_MASK) != 0U)
  {
    return VL6180X_STATUS_RANGE_ERROR;
  }

  *distance_mm = measured_distance;
  device->measure_stage = VL6180X_MEASURE_STAGE_DONE;
  return VL6180X_STATUS_OK;
}
