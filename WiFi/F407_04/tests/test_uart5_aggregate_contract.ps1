$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Raw (Join-Path $root 'User/main.c')
$uart5 = Get-Content -Raw (Join-Path $root 'User/usart_protocol/bsp_uart5_hall.c')
$protocol = Get-Content -Raw (Join-Path $root 'User/usart_protocol/usart_protocol.c')
$header = Get-Content -Raw (Join-Path $root 'User/usart_protocol/usart_protocol.h')
$project = Get-Content -Raw (Join-Path $root 'Project/Fire_F407ZG.uvprojx')

if ($main -match 'I2C3_HALL_Config\s*\(' -or $main -match 'SensorFrame_PollI2C3\s*\(') {
    throw 'Upper firmware still initializes or polls the F407HALL I2C3 link.'
}
if ($project -match 'bsp_i2c3_hall\.c' -or $project -match 'sensor_frame\.c') {
    throw 'Upper Keil project still links the retired I2C3 HALL path.'
}
if ($uart5 -notmatch 'Protocol_ParseByte\s*\(\s*UART_IDX_UART5_HALL\s*,\s*byte\s*\)') {
    throw 'UART5 does not feed bytes into the common parser.'
}
if ($header -notmatch 'MODULE_ID_CHULIAO\s+0x05' -or
    $header -notmatch 'MODULE_ID_HALL_VL6180X\s+0x03' -or
    $header -notmatch 'FRAME_HALL_VL6180X_LEN\s+16') {
    throw 'Parser header does not expose the original 0x05 and 0x03 frame contracts.'
}
if ($protocol -notmatch 'byte\s*==\s*MODULE_ID_HALL_VL6180X' -or
    $protocol -notmatch 'Protocol_ParseHallVL6180XFrame') {
    throw 'Common parser does not accept the HALL frame on a UART byte stream.'
}
if ($protocol -notmatch 'valid\s*\|=\s*0x02U' -or $protocol -notmatch 'valid\s*\|=\s*0x08U') {
    throw 'Modbus C/D valid-bit mapping changed.'
}

Write-Host 'PASS: upper firmware receives both chuliao and HALL frames through UART5 only.'
