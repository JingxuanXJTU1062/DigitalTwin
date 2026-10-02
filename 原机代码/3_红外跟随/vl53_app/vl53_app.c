#include "vl53_app.h"

#include "delay.h"
#include "stdio.h"

#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"
#include "stm32_i2c.h"
#define VL53_I2C_ADDR   0x52
#define PERIOD_VL53_MS  100
static VL53L0X_Dev_t dev;
static VL53L0X_Dev_t *pdev = &dev;

static volatile uint16_t g_dist_raw_mm  = 0;
static volatile uint16_t g_dist_filt_mm = 0;
static volatile uint8_t  g_dist_ok      = 0;

static int I2C_Ping_8bit(uint8_t addr_8bit)
{
    return (i2c_probe(addr_8bit) == 0) ? 1 : 0;
}

// ===== ˲״̬ã=====
static uint32_t g_filt_y = 0;

static void dist_filter_reset(void)
{
    g_filt_y = 0;
}

static uint16_t dist_filter_update(uint16_t raw)
{
    // һν raw ʼ
    if (g_filt_y == 0) g_filt_y = raw;

    
    g_filt_y = (g_filt_y * 3u + (uint32_t)raw * 7u) / 10u;

    return (uint16_t)g_filt_y;
}


bool VL53_App_Init(void)
{
    VL53L0X_Error st;

    i2c_init();
    delay_ms(300);

    if (!I2C_Ping_8bit(VL53_I2C_ADDR)) {
        printf("VL53 I2C ping failed\r\n");
        return false;
    }

    pdev->I2cDevAddr = VL53_I2C_ADDR;

    st = VL53L0X_DataInit(pdev);
    if (st != VL53L0X_ERROR_NONE) return false;

    st = VL53L0X_StaticInit(pdev);
    if (st != VL53L0X_ERROR_NONE) return false;

    uint8_t VhvSettings = 0;
    uint8_t PhaseCal = 0;
     st = VL53L0X_PerformRefCalibration(pdev, &VhvSettings, &PhaseCal);
    if (st != VL53L0X_ERROR_NONE) return false;

    uint32_t refSpadCount = 0;
    uint8_t isApertureSpads = 0;
    st = VL53L0X_PerformRefSpadManagement(pdev, &refSpadCount, &isApertureSpads);
    if (st != VL53L0X_ERROR_NONE) return false;

    VL53L0X_SetDeviceMode(pdev, VL53L0X_DEVICEMODE_CONTINUOUS_RANGING);
    VL53L0X_SetMeasurementTimingBudgetMicroSeconds(pdev, 33000);
    VL53L0X_SetInterMeasurementPeriodMilliSeconds(pdev, PERIOD_VL53_MS);

    st = VL53L0X_StartMeasurement(pdev);
    if (st != VL53L0X_ERROR_NONE) return false;
    // ʼɹһ״̬
    g_dist_ok = 0;
    g_dist_raw_mm = 0;
    g_dist_filt_mm = 0;
    dist_filter_reset();

    return true;
}

bool VL53_App_Update(void)
{
    VL53L0X_Error st;
    uint8_t ready = 0;

    st = VL53L0X_GetMeasurementDataReady(pdev, &ready);
    if (st != VL53L0X_ERROR_NONE) {
        return false;   // 
    }

    if (!ready) {
        return true;    
    }

    VL53L0X_RangingMeasurementData_t data;
    st = VL53L0X_GetRangingMeasurementData(pdev, &data);

    if (st != VL53L0X_ERROR_NONE) {
        g_dist_ok = 0;
        return false;   
    }

    if (data.RangeStatus == 0) {
        uint16_t raw = data.RangeMilliMeter;

        
        if (raw > 100 && raw < 300) {
            g_dist_raw_mm  = raw;
            g_dist_filt_mm = dist_filter_update(raw);
            g_dist_ok      = 1;
        } else {
            g_dist_ok = 0;   
        }
    } else {
        g_dist_ok = 0;       
    }


    st = VL53L0X_ClearInterruptMask(
        pdev,
        VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY
    );
    if (st != VL53L0X_ERROR_NONE) {
        return false;
    }


    return true;
}




bool VL53_App_IsDataValid(void)
{
    return g_dist_ok;
}

uint16_t VL53_App_GetRawMM(void)
{
    return g_dist_raw_mm;
}

uint16_t VL53_App_GetFiltMM(void)
{
    return g_dist_filt_mm + 20;
}

static void VL53_App_RangingConfig(void)
{
    VL53L0X_SetDeviceMode(pdev, VL53L0X_DEVICEMODE_CONTINUOUS_RANGING);
    VL53L0X_SetMeasurementTimingBudgetMicroSeconds(pdev, 33000);
    VL53L0X_SetInterMeasurementPeriodMilliSeconds(pdev, PERIOD_VL53_MS);
}

void VL53_App_I2CRecover(void)
{
    i2c_bus_recover();
    i2c_init();
}

// Stop/Start
static bool VL53_App_RestartLight(void)
{
    if (VL53L0X_StopMeasurement(pdev) != VL53L0X_ERROR_NONE) return false;
    delay_ms(10);
    if (VL53L0X_StartMeasurement(pdev) != VL53L0X_ERROR_NONE) return false;

    // ɹ˲־ֵβ
    g_dist_ok = 0;
    dist_filter_reset();
    return true;
}


// سʼDataInit + StaticInit +  + Start
static bool VL53_App_RestartHard(void)
{
    VL53L0X_StopMeasurement(pdev);
    delay_ms(10);

    if (VL53L0X_DataInit(pdev) != VL53L0X_ERROR_NONE) return false;
    if (VL53L0X_StaticInit(pdev) != VL53L0X_ERROR_NONE) return false;

    // ѡȣԭ init ˣ
    uint8_t VhvSettings = 0, PhaseCal = 0;
    VL53L0X_PerformRefCalibration(pdev, &VhvSettings, &PhaseCal);

    uint32_t refSpadCount = 0;
    uint8_t isApertureSpads = 0;
    VL53L0X_PerformRefSpadManagement(pdev, &refSpadCount, &isApertureSpads);

    VL53_App_RangingConfig();

    if (VL53L0X_StartMeasurement(pdev) != VL53L0X_ERROR_NONE) return false;
		g_dist_ok = 0;
    dist_filter_reset();

    return true;
}

// ⣺ڣ main У
void VL53_App_RestartRanging(void)
{
    // Ȼָ I2Cرǵų
    VL53_App_I2CRecover();

    // ʧӲ
    if (!VL53_App_RestartLight()) {
        VL53_App_RestartHard();
    }
}

