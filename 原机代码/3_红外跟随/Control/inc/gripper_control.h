#ifndef __GRIPPER_CONTROL_H
#define __GRIPPER_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TENSION_INIT_GAP_MM
#define TENSION_INIT_GAP_MM         (15.0f)
#endif

#ifndef TENSION_SENSOR_BASELINE_MM
#define TENSION_SENSOR_BASELINE_MM  (140.0f)
#endif

#ifndef TENSION_DELTA_SIGN
#define TENSION_DELTA_SIGN          (+1.0f)
#endif

#ifndef TENSION_ENTER_DELTA_MM
#define TENSION_ENTER_DELTA_MM      (10.0f)
#endif

#ifndef TENSION_EXIT_DELTA_MM
#define TENSION_EXIT_DELTA_MM       (6.0f)
#endif

#ifndef GRIPPER_CLEARANCE_MM
#define GRIPPER_CLEARANCE_MM        (0.0f)
#endif

#ifndef GRIPPER_GAP_MIN_MM
#define GRIPPER_GAP_MIN_MM          (15.0f)
#endif

#ifndef shuazi_width
#define shuazi_width                (100.0f)
#endif

#ifndef GRIPPER_GAP_MAX_MM
#define GRIPPER_GAP_MAX_MM          (70.0f)
#endif

#define GRIPPER_STEPS_PER_MM        800

#ifndef GRIPPER_LEFT_ADDR
#define GRIPPER_LEFT_ADDR           (1u)
#endif



#ifndef GRIPPER_LEFT_INWARD_DIR
#define GRIPPER_LEFT_INWARD_DIR     (1u)
#endif


#ifndef GRIPPER_CLOSE_VEL_RPM
#define GRIPPER_CLOSE_VEL_RPM       (200u)
#endif

#ifndef GRIPPER_OPEN_VEL_RPM
#define GRIPPER_OPEN_VEL_RPM        (500u)
#endif

#ifndef GRIPPER_ACC
#define GRIPPER_ACC                 (0u)
#endif

#ifndef GRIPPER_ENTER_COUNT
#define GRIPPER_ENTER_COUNT         (4u)
#endif

#ifndef GRIPPER_EXIT_COUNT
#define GRIPPER_EXIT_COUNT          (5u)
#endif

#ifndef GRIPPER_WIDTH_BUF_LEN
#define GRIPPER_WIDTH_BUF_LEN       (256u)
#endif

#ifndef GRIPPER_IR_LOST_EXIT_COUNT
#define GRIPPER_IR_LOST_EXIT_COUNT  (10u)
#endif

#ifndef GRIPPER_GAP_DEADBAND_MM
#define GRIPPER_GAP_DEADBAND_MM     (3.0f)
#endif

#ifndef GRIPPER_REOPEN_ON_EXIT
#define GRIPPER_REOPEN_ON_EXIT      (0u)
#endif

typedef enum {
    GRIPPER_STATE_WAITING  = 0,
    GRIPPER_STATE_TRACKING = 1
} GripperState_e;

typedef struct {
    uint8_t valid;
    float   width_mm;
} GripperWidthSample_t;

typedef struct {
    uint32_t current_inset_steps;
    float    last_cmd_gap_mm;
    bool     fish_present;
    GripperState_e state;

    uint16_t  enter_cnt;
    uint16_t  exit_cnt;
    uint16_t  ir_lost_cnt;

    uint8_t  capture_active;
    uint8_t  tracking_started;

    GripperWidthSample_t width_buf[GRIPPER_WIDTH_BUF_LEN];
    uint16_t width_wr_idx;
    uint16_t width_rd_idx;
    uint16_t width_count;
} Gripper_t;

void Gripper_Init(Gripper_t *g);
float Gripper_EstimateObjectWidthMM(uint16_t distance_mm);
bool Gripper_SetGapMM_Rel(Gripper_t *g, float gap_mm, uint16_t vel_rpm, uint8_t acc);
void Gripper_Update(Gripper_t *g, bool dist_valid, uint16_t filt_dist_mm, bool ir_blocked);
bool Gripper_MoveRelSteps(int32_t delta_steps, uint16_t vel_rpm, uint8_t acc);

#ifdef __cplusplus
}
#endif

#endif
