$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$bridgeH = Join-Path $root 'xiaweiji/F103chuliao05/Core/Inc/hall_bridge.h'
$bridgeC = Join-Path $root 'xiaweiji/F103chuliao05/Core/Src/hall_bridge.c'
$protocolC = Join-Path $root 'xiaweiji/F103chuliao05/Core/Src/usart_protocol.c'
$mainC = Join-Path $root 'xiaweiji/F103chuliao05/Core/Src/main.c'

if (-not (Test-Path $bridgeH) -or -not (Test-Path $bridgeC)) {
    throw 'hall_bridge.h/c do not exist.'
}

$header = Get-Content -Raw $bridgeH
$source = Get-Content -Raw $bridgeC
$protocol = Get-Content -Raw $protocolC
$main = Get-Content -Raw $mainC

foreach ($symbol in @('HallBridge_Init', 'HallBridge_PushByte', 'HallBridge_CopyFreshFrame')) {
    if ($header -notmatch $symbol -or $source -notmatch $symbol) {
        throw "Missing bridge interface: $symbol"
    }
}
if ($header -notmatch 'HALL_BRIDGE_FRAME_SIZE\s+16U' -or
    $source -notmatch 'HALL_BRIDGE_MODULE_ID\s+0x03U' -or
    $source -notmatch 'HALL_BRIDGE_PAYLOAD_LEN\s+0x0BU') {
    throw 'Bridge does not enforce the original 16-byte 0x03/0x0B frame.'
}
if ($source -notmatch 'for\s*\(\s*i\s*=\s*2U\s*;\s*i\s*<\s*15U') {
    throw 'Bridge XOR range is not bytes 2 through 14.'
}
if ($source -notmatch 'now_ms\s*-\s*g_hall_bridge_timestamp\)\s*>\s*timeout_ms') {
    throw 'Bridge freshness rule does not keep age <= timeout and reject age > timeout.'
}
if ($protocol -notmatch 'g_tx_batch\s*\[\s*28\s*\]' -or
    $protocol -notmatch 'g_tx_batch_len\s*=\s*28U') {
    throw 'Protocol has no 28-byte aggregate snapshot.'
}
if ($protocol -notmatch 'g_tx_batch\[i\]\s*=\s*g_tx_frame\[i\]' -or
    $protocol -notmatch 'g_tx_batch\[12U\s*\+\s*i\]\s*=\s*hall_frame\[i\]') {
    throw 'Aggregate order is not 0x05 frame followed by 0x03 frame.'
}
if ($protocol -notmatch 'HallBridge_CopyFreshFrame\s*\([^;]*500U') {
    throw 'Protocol does not enforce the 500 ms HALL timeout.'
}
if ($main -notmatch 'HallBridge_Init\s*\(\s*&huart1\s*\)' -or
    $main -notmatch 'HAL_UART_RxCpltCallback' -or
    $main -notmatch 'HallBridge_RxCpltCallback') {
    throw 'USART1 RX callbacks are not wired to the HALL bridge.'
}

Write-Host 'PASS: chuliao HALL bridge and 12/28-byte batching contract.'
