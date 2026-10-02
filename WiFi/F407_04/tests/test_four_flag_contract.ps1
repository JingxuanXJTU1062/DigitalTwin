$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'User\ESP8266\wifi_cmd.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'User\ESP8266\wifi_cmd.c')
$modbus = Get-Content -Raw -LiteralPath (Join-Path $root 'User\Modbus\modbus_tcp_server.h')

$checks = @(
    @{ Name = 'four Modbus coils'; Ok = $modbus -match '#define\s+MODBUS_COIL_COUNT\s+4U' },
    @{ Name = 'Flag4 Watch variable'; Ok = $header -match 'g_cmd_flag4' },
    @{ Name = 'Flag4 uses PC2'; Ok = $source -match 'WIFI_CMD_PIN4\s+GPIO_PIN_2' -and $source -match 'WIFI_CMD_PORT4\s+GPIOC' },
    @{ Name = 'four-value parser'; Ok = $source -match 'sscanf\(tmp,\s*"CMD,%d,%d,%d,%d%c"' },
    @{ Name = 'strict binary values'; Ok = $source -match '(?s)i1\s*>\s*1.*i2\s*>\s*1.*i3\s*>\s*1.*i4\s*>\s*1' }
)

$failed = @($checks | Where-Object { -not $_.Ok })
if ($failed.Count -gt 0) {
    $failed | ForEach-Object { Write-Error ("Missing contract: " + $_.Name) }
    exit 1
}

Write-Output 'PASS: F407 exposes four strict command flags on PE4/PE5/PE6/PC2.'
