$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$hall = Join-Path $root 'xiaweiji\F407HALL'
$chuliao = Join-Path $root 'xiaweiji\F103chuliao05'
$wifi = Join-Path $root 'WiFi\F407_04'

function Require-Match([string]$Path, [string]$Pattern, [string]$Message) {
    $text = Get-Content -Raw $Path
    if ($text -notmatch $Pattern) { throw $Message }
}

Require-Match (Join-Path $hall 'Core\Inc\distance_tx.h') 'DistanceTx_Init\(UART_HandleTypeDef \*uart\)' 'F407HALL must expose a UART transport'
Require-Match (Join-Path $hall 'Core\Src\distance_tx.c') 'HAL_UART_Transmit' 'F407HALL must send its unchanged frame over UART'
Require-Match (Join-Path $hall 'Core\Src\usart.c') 'PA9\s+------> USART1_TX' 'F407HALL must use PA9 USART1 TX'

Require-Match (Join-Path $chuliao 'Core\Src\hall_bridge.c') 'HallBridge_PushByte' 'F103chuliao05 must parse the HALL UART frame'
Require-Match (Join-Path $chuliao 'Core\Src\usart_protocol.c') 'g_tx_batch\[28\]' 'F103chuliao05 must provide a 28-byte aggregate buffer'
Require-Match (Join-Path $chuliao 'Core\Src\usart_protocol.c') 'HallBridge_CopyFreshFrame' 'F103chuliao05 must append fresh HALL data'

Require-Match (Join-Path $wifi 'User\usart_protocol\usart_protocol.c') 'MODULE_ID_HALL_VL6180X' 'Upper UART parser must accept the 0x03 frame'
Require-Match (Join-Path $wifi 'User\usart_protocol\usart_protocol.c') 'FRAME_HALL_VL6180X_LEN' 'Upper UART parser must use the unchanged 16-byte length'
Require-Match (Join-Path $wifi 'User\usart_protocol\bsp_uart5_hall.c') 'Protocol_ParseByte\(UART_IDX_UART5_HALL' 'Upper firmware must receive the aggregate stream through UART5'

Write-Output 'transport reassignment checks passed'
