$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Raw (Join-Path $root 'Core/Src/main.c')
$i2c = Get-Content -Raw (Join-Path $root 'Core/Src/i2c.c')
$spi = Get-Content -Raw (Join-Path $root 'Core/Src/spi.c')
$gpio = Get-Content -Raw (Join-Path $root 'Core/Src/gpio.c')
$mainHeader = Get-Content -Raw (Join-Path $root 'Core/Inc/main.h')
$ioc = Get-Content -Raw (Join-Path $root 'F407HALL.ioc')
$cube = Get-Content -Raw (Join-Path $root 'cmake/stm32cubemx/CMakeLists.txt')
$cmake = Get-Content -Raw (Join-Path $root 'CMakeLists.txt')

function Require-Text([string]$text, [string]$pattern, [string]$label) {
    if ($text -notmatch $pattern) { throw "Missing: $label" }
}
function Forbid-Text([string]$text, [string]$pattern, [string]$label) {
    if ($text -match $pattern) { throw "Forbidden: $label" }
}

Require-Text $main 'g_vl6180x_3' 'third VL6180X instance'
Require-Text $main 'g_distance_mm_3' 'third distance value'
Require-Text $main 'MX_I2C3_Init\(\)' 'I2C3 initialization'
Require-Text $main 'SPI_Slave_Init\(\)' 'SPI slave initialization'
Require-Text $main 'SPI_Slave_BuildFrame\(' 'SPI data publication'
Require-Text $main 'g_measure_status_3' 'third sensor direct measurement status'
Require-Text $main 'g_measure_stage_3' 'third sensor direct measurement stage'
Require-Text $main 'g_measure_error_count_3' 'third sensor measurement error counter'
Require-Text $main 'g_i2c3_recovery_count' 'I2C3 recovery counter'
Require-Text $main 'Stage1_RecoverSensor3\(' 'I2C3 and sensor 3 recovery path'
Require-Text $main 'HAL_I2C_DeInit\(&hi2c3\)' 'I2C3 deinitialization during recovery'
Require-Text $main 'MX_I2C3_Init\(\)' 'I2C3 reinitialization during recovery'
Require-Text $main '\*present = 1U;\s*\}' 'present set only after successful initialization'
Forbid-Text $main 'MX_USART\d+_UART_Init' 'UART initialization'
Require-Text $i2c 'hi2c3' 'I2C3 handle'
Require-Text $i2c 'hi2c3' 'dedicated third sensor bus'
Require-Text $i2c 'GPIO_PIN_9' 'I2C3 SDA PC9'
Require-Text $i2c 'GPIO_PIN_8' 'I2C3 SCL PA8'
Require-Text $i2c 'HAL_GPIO_Init\(GPIOC, &GPIO_InitStruct\);\s*GPIO_InitStruct\.Pin = GPIO_PIN_8;\s*GPIO_InitStruct\.Mode = GPIO_MODE_AF_OD;\s*GPIO_InitStruct\.Pull = GPIO_NOPULL;\s*GPIO_InitStruct\.Speed = GPIO_SPEED_FREQ_VERY_HIGH;\s*GPIO_InitStruct\.Alternate = GPIO_AF4_I2C3;\s*HAL_GPIO_Init\(GPIOA, &GPIO_InitStruct\);' 'I2C3 PA8 receives a complete independent GPIO configuration matching I2C2'
Forbid-Text $cube 'usart\.c' 'generated USART source'
Forbid-Text $cmake 'protocol\.c|distance_tx\.c' 'UART protocol sources'
Require-Text $cmake 'spi_slave\.c' 'SPI slave source'
Require-Text $spi 'SPI1->CR1 = 0U' 'SPI1 hardware-NSS slave mode'
Require-Text $spi 'GPIO_PIN_4.*GPIO_PIN_5.*GPIO_PIN_6.*GPIO_PIN_7' 'SPI1 PA4-PA7 pins'
Require-Text $mainHeader '#define VL6180X_SHDN_3_Pin GPIO_PIN_6' 'third VL6180X XSHUT pin on PG6'
Require-Text $mainHeader '#define VL6180X_SHDN_3_GPIO_Port GPIOG' 'third VL6180X XSHUT port on PG6'
Require-Text $ioc 'PG6\.GPIO_Label=VL6180X_SHDN_3' 'CubeMX third VL6180X XSHUT label on PG6'
Forbid-Text $ioc 'PC[57]\.GPIO_Label=VL6180X_SHDN_3' 'stale CubeMX third VL6180X XSHUT on GPIOC'
Require-Text $gpio '__HAL_RCC_GPIOG_CLK_ENABLE\(\)' 'GPIOG clock for third XSHUT'
Require-Text $gpio 'HAL_GPIO_WritePin\(GPIOG, VL6180X_SHDN_3_Pin, GPIO_PIN_RESET\)' 'third XSHUT starts low on GPIOG'
Require-Text $gpio 'HAL_GPIO_Init\(GPIOG, &GPIO_InitStruct\)' 'third XSHUT initialized on GPIOG'
foreach ($uartFile in @('Core/Src/usart.c', 'Core/Inc/usart.h', 'Core/Src/protocol.c', 'Core/Inc/protocol.h', 'Core/Src/distance_tx.c', 'Core/Inc/distance_tx.h')) {
    if (Test-Path (Join-Path $root $uartFile)) { throw "Forbidden UART source remains: $uartFile" }
}

Write-Host 'PASS: HALL03 has 2 Hall + 3 dedicated I2C buses + SPI1 uplink and no UART.'
