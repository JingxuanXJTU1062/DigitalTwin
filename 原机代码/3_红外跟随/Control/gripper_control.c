#include "gripper_control.h"
#include "Emm_V5.h"
#include <math.h>
#include "usart.h"
#include "delay.h"
#include "board.h"
#include <stdio.h>

#define MOTOR_STEPS_PER_REV   3200.0f
#define MOTOR_MM_PER_REV      4.0f
#define MOTOR_DEG_PER_STEP    (360.0f / MOTOR_STEPS_PER_REV)
#define MOTOR_MM_PER_DEG      (MOTOR_MM_PER_REV / 360.0f)

static bool motor_read_angle_deg(uint8_t addr, float *deg_out)
{
    uint32_t pos = 0;
    float angle = 0.0f;

    /* 1. 优先使用串口中断缓存的自动返回位置 */
    if (usart_GetMotorAngleDeg(addr, deg_out))
    {
//		printf("cache angle[%u] = %.2f\r\n", addr, *deg_out);
        return true;
    }

    /* 2. 如果暂时还没收到自动返回帧，再回退到主动读取一次 */
    rxFrameFlag = false;
    Emm_V5_Read_Sys_Params(addr, S_CPOS);

    {
        uint32_t timeout = 200u;
        while(!rxFrameFlag && timeout--) delay_ms(1);
        if(!rxFrameFlag) return false;
        rxFrameFlag = false;
    }

    if(rxCmd[0] == addr && rxCmd[1] == 0x36 && rxCount == 8)
    {
        pos = ((uint32_t)rxCmd[3] << 24) |
              ((uint32_t)rxCmd[4] << 16) |
              ((uint32_t)rxCmd[5] << 8)  |
              ((uint32_t)rxCmd[6]);
        angle = (float)pos * 360.0f / 65536.0f;
        if(rxCmd[2]) angle = -angle;
        *deg_out = angle;
		// printf("fallback read angle\r\n");
        return true;
    }

    return false;
}

static float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static uint8_t invert_dir(uint8_t dir)
{
    return (dir == 0u) ? 1u : 0u;
}

static uint32_t mm_to_steps(float mm)
{
    if (mm <= 0.0f) return 0u;
    return (uint32_t)lroundf(mm * (float)GRIPPER_STEPS_PER_MM);
}

static float distance_to_open_delta_mm(uint16_t distance_mm)
{
    float delta = TENSION_DELTA_SIGN * ((float)distance_mm - (float)TENSION_SENSOR_BASELINE_MM);
    if (delta < 0.0f) delta = 0.0f;
    return delta;
}

static uint8_t object_present_from_width(float object_width_mm)
{
    float delta = object_width_mm - TENSION_INIT_GAP_MM;
    return (delta >= TENSION_ENTER_DELTA_MM) ? 1u : 0u;
}
static void width_buf_reset(Gripper_t *g)
{
    uint16_t i;
    if (!g) return;
	
    g->width_wr_idx = 0u;
    g->width_rd_idx = 0u;
    g->width_count  = 0u;

    for (i = 0u; i < GRIPPER_WIDTH_BUF_LEN; ++i) {
        g->width_buf[i].valid = 0u;
        g->width_buf[i].width_mm = TENSION_INIT_GAP_MM;
    }
}

static void width_buf_push(Gripper_t *g, uint8_t valid, float width_mm)
{
    if (!g) return;

    g->width_buf[g->width_wr_idx].valid = valid;
    g->width_buf[g->width_wr_idx].width_mm = width_mm;

    g->width_wr_idx++;
    if (g->width_wr_idx >= GRIPPER_WIDTH_BUF_LEN) {
        g->width_wr_idx = 0u;
    }

    if (g->width_count < GRIPPER_WIDTH_BUF_LEN) {
        g->width_count++;
    } else {
        g->width_rd_idx++;
        if (g->width_rd_idx >= GRIPPER_WIDTH_BUF_LEN) {
            g->width_rd_idx = 0u;
        }
    }
}

static GripperWidthSample_t width_buf_pop(Gripper_t *g)
{
    GripperWidthSample_t out = {0u, TENSION_INIT_GAP_MM};

    if (!g) return out;
    if (g->width_count == 0u) return out;

    out = g->width_buf[g->width_rd_idx];

    g->width_rd_idx++;
    if (g->width_rd_idx >= GRIPPER_WIDTH_BUF_LEN) {
        g->width_rd_idx = 0u;
    }

    g->width_count--;
    return out;
}


static float gap_to_inset_mm(float gap_mm)
{
    float cur_deg = 0.0f;
    float width = shuazi_width;
    gap_mm = clampf(gap_mm, GRIPPER_GAP_MIN_MM, GRIPPER_GAP_MAX_MM);
    if(motor_read_angle_deg(GRIPPER_LEFT_ADDR, &cur_deg))
    {
        width = shuazi_width - fabsf(cur_deg) * MOTOR_MM_PER_DEG;
    }
    {
        float inset = (width - gap_mm) * 0.5f;
        if (inset < 0.0f) inset = 0.0f;
        return inset;
    }
}

static uint32_t max_inset_steps(void)
{
    return mm_to_steps(gap_to_inset_mm(GRIPPER_GAP_MIN_MM));
}

static bool move_both_rel(int32_t delta_steps, uint16_t vel_rpm, uint8_t acc)
{
    uint32_t steps;
    float move_deg;
    uint8_t left_dir;
    float start_deg = 0.0f;
    float dir_sign = -1.0f;
    float target_deg;

    if (delta_steps == 0) return true;

    steps = (delta_steps > 0) ? (uint32_t)delta_steps : (uint32_t)(-delta_steps);
    move_deg = (float)steps * MOTOR_DEG_PER_STEP;
    left_dir = (delta_steps > 0) ? (uint8_t)GRIPPER_LEFT_INWARD_DIR : invert_dir((uint8_t)GRIPPER_LEFT_INWARD_DIR);

    if(!motor_read_angle_deg(GRIPPER_LEFT_ADDR, &start_deg)) return false;

    target_deg = start_deg + dir_sign * ((delta_steps > 0) ? move_deg : -move_deg);
    Emm_V5_Pos_Control(GRIPPER_LEFT_ADDR, left_dir, vel_rpm, acc, steps, 2, 0);
    delay_ms(50);

    {
        float cur_deg = start_deg;
        float err = 9999.0f;
        float revs = (float)steps / MOTOR_STEPS_PER_REV;
        float time_s = (60.0f * revs) / (float)vel_rpm;
        uint32_t timeout_ms = (uint32_t)(time_s * 2000.0f) + 200u;
        while(timeout_ms > 0u)
        {
            delay_ms(20);
            timeout_ms -= 20u;
            if(!motor_read_angle_deg(GRIPPER_LEFT_ADDR, &cur_deg)) continue;
            err = fabsf(target_deg - cur_deg);
            if(err <= 3.0f) return true;
        }
    }

    return false;
}

void Gripper_Init(Gripper_t *g)
{
    if (!g) return;

    g->current_inset_steps = 0u;
    g->last_cmd_gap_mm     = shuazi_width;
    g->fish_present        = false;
    g->state               = GRIPPER_STATE_WAITING;

    g->enter_cnt           = 0u;
    g->exit_cnt            = 0u;
    g->ir_lost_cnt         = 0u;

    g->capture_active      = 0u;
    g->tracking_started    = 0u;

    width_buf_reset(g);
	usart_ClearMotorAngleCache(GRIPPER_LEFT_ADDR);
    motor_set_auto_return_pos(GRIPPER_LEFT_ADDR, 50u);
    delay_ms(300);
}

float Gripper_EstimateObjectWidthMM(uint16_t distance_mm)
{
    float w = TENSION_INIT_GAP_MM + distance_to_open_delta_mm(distance_mm);
    return clampf(w, TENSION_INIT_GAP_MM, GRIPPER_GAP_MAX_MM);
}

bool Gripper_SetGapMM_Rel(Gripper_t *g, float gap_mm, uint16_t vel_rpm, uint8_t acc)
{
    uint32_t target_steps;
    uint32_t max_steps;
    int32_t delta;
    if (!g) return false;
    gap_mm = clampf(gap_mm, GRIPPER_GAP_MIN_MM, GRIPPER_GAP_MAX_MM);
    if (fabsf(gap_mm - g->last_cmd_gap_mm) < GRIPPER_GAP_DEADBAND_MM) return true;
    target_steps = mm_to_steps(gap_to_inset_mm(gap_mm));
    max_steps = max_inset_steps();
    if (target_steps > max_steps) target_steps = max_steps;
    delta = (int32_t)target_steps - (int32_t)g->current_inset_steps;
    if ((delta > 0) && (g->current_inset_steps >= max_steps)) return true;
    if ((delta < 0) && (g->current_inset_steps == 0u)) return true;
    if(move_both_rel(delta, vel_rpm, acc))
    {
        g->current_inset_steps = target_steps;
        g->last_cmd_gap_mm = gap_mm;
        return true;
    }
    return false;
}

void Gripper_Update(Gripper_t *g, bool dist_valid, uint16_t filt_dist_mm, bool ir_blocked)
{
    float front_width_mm;
    uint8_t front_present;
    GripperWidthSample_t sample;
    float gap;

    if (!g) return;
    /* 1. 前端张紧装置测得当前物体宽度 */
    if (dist_valid) 
	{
        front_width_mm = Gripper_EstimateObjectWidthMM(filt_dist_mm);
        front_present = object_present_from_width(front_width_mm);
    }
	else 
	{
        front_width_mm = TENSION_INIT_GAP_MM;
        front_present = 0u;
    }
    /* 2. 前端进入/离开计数 */
    if (front_present) 
	{
        if (g->enter_cnt < 400u) g->enter_cnt++;
        g->exit_cnt = 0u;
    } 
	else 
	{
        if (g->exit_cnt < 400u) g->exit_cnt++;
        g->enter_cnt = 0u;
    }

    /* 3. 前端检测到物体的次数超过阈值，就开启宽度缓存 */
    if ((g->capture_active == 0u) && front_present && g->enter_cnt >= (uint8_t)GRIPPER_ENTER_COUNT ) {
        g->capture_active   = 1u;
        g->tracking_started = 0u;
        g->fish_present     = true;
        g->ir_lost_cnt      = 0u;
        width_buf_reset(g);

        printf("front object detected, start width capture\r\n");
    }

    /* 4. 缓存开启后，持续把前端测到的宽度写入 FIFO */
    if (g->capture_active) {
        width_buf_push(g, front_present, front_width_mm);
    }

    /* 5. WAITING 状态下，只要 PA7(IR) 检测到遮挡，且缓存里已有数据，
          就立刻进入 TRACKING */
    if (g->state == GRIPPER_STATE_WAITING) {
        if (ir_blocked && (g->width_count > 0u)) {
            g->state = GRIPPER_STATE_TRACKING;
            g->tracking_started = 1u;
            g->ir_lost_cnt = 0u;
            printf("IR blocked, start gripper tracking\r\n");
        } else {
            return;
        }
    }

    /* 6. TRACKING 状态下，统计 IR 丢失情况 */
    if (!ir_blocked) {
        if (g->ir_lost_cnt < 255u) g->ir_lost_cnt++;
    } else {
        g->ir_lost_cnt = 0u;
    }

    /* 7. 从 FIFO 中取一个宽度样本，动态调节夹持口 */
    sample = width_buf_pop(g);

    if (sample.valid) {
        gap = sample.width_mm + GRIPPER_CLEARANCE_MM;
        gap = clampf(gap, GRIPPER_GAP_MIN_MM, GRIPPER_GAP_MAX_MM);
        (void)Gripper_SetGapMM_Rel(g, gap, GRIPPER_CLOSE_VEL_RPM, (uint8_t)GRIPPER_ACC);
    }

    /* 8. 退出条件：
          前端已经连续判断物体离开，并且 IR 也连续恢复无遮挡 */
    if ((g->exit_cnt >= (uint8_t)GRIPPER_EXIT_COUNT) &&
        (g->ir_lost_cnt >= (uint8_t)GRIPPER_IR_LOST_EXIT_COUNT)) {

        g->fish_present     = false;
        g->state            = GRIPPER_STATE_WAITING;
        g->enter_cnt        = 0u;
        g->exit_cnt         = 0u;
        g->ir_lost_cnt      = 0u;
        g->capture_active   = 0u;
        g->tracking_started = 0u;

        width_buf_reset(g);

#if (GRIPPER_REOPEN_ON_EXIT != 0)
        (void)Gripper_SetGapMM_Rel(g, GRIPPER_GAP_MAX_MM, GRIPPER_OPEN_VEL_RPM, (uint8_t)GRIPPER_ACC);
#endif

        printf("object leave, stop tracking\r\n");
    }
}

bool Gripper_MoveRelSteps(int32_t delta_steps, uint16_t vel_rpm, uint8_t acc)
{
    return move_both_rel(delta_steps, vel_rpm, acc);
}
