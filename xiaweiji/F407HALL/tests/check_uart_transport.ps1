$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw (Join-Path $root 'Core/Inc/distance_tx.h')
$source = Get-Content -Raw (Join-Path $root 'Core/Src/distance_tx.c')
$main = Get-Content -Raw (Join-Path $root 'Core/Src/main.c')

if ($header -notmatch 'DistanceTx_Init\s*\(\s*UART_HandleTypeDef\s*\*') {
    throw 'DistanceTx_Init does not accept a UART handle.'
}
if ($source -notmatch 'HAL_UART_Transmit\s*\(') {
    throw 'DistanceTx does not transmit through HAL UART.'
}
if ($source -notmatch 'HAL_UART_Transmit\s*\([^;]*DISTANCE_TX_FRAME_SIZE') {
    throw 'DistanceTx UART send is not fixed to the 16-byte frame size.'
}
if ($source -match 'HAL_I2C_Slave_Transmit_IT') {
    throw 'DistanceTx still contains the I2C slave transport.'
}
if ($main -notmatch 'MX_USART1_UART_Init\s*\(\s*\)') {
    throw 'F407HALL does not initialize USART1.'
}
if ($main -notmatch 'DistanceTx_Init\s*\(\s*&huart1\s*\)') {
    throw 'F407HALL does not bind DistanceTx to USART1.'
}
if ($main -match 'MX_I2C3_Init\s*\(\s*\)') {
    throw 'F407HALL still initializes I2C3 for the board link.'
}

Write-Host 'PASS: F407HALL uses USART1 for the unchanged 16-byte frame.'
