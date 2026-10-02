$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

function Read-ProjectFile([string]$module, [string]$relative) {
    Get-Content -Raw -LiteralPath (Join-Path $root ("原机代码\{0}\{1}" -f $module, $relative))
}

$feedMain = Read-ProjectFile '1_进料' 'APP\main.c'
$feedTouch = Read-ProjectFile '1_进料' 'Hardware\TTP223.c'
$descaleMain = Read-ProjectFile '2_去鳞' 'APP\main.c'
$descaleTouch = Read-ProjectFile '2_去鳞' 'Hardware\TTP223.c'
$gutMain = Read-ProjectFile '4_开膛去内脏' 'APP\main.c'
$gutTouch = Read-ProjectFile '4_开膛去内脏' 'Hardware\TTP223.c'
$dischargeMain = Read-ProjectFile '5_出料' 'APP\main.c'
$dischargeTouch = Read-ProjectFile '5_出料' 'Hardware\TTP223.c'

$checks = @(
    @{ Name = 'feed polls PA6 level'; Ok = $feedMain -match 'TTP223_ReadState\(\)' },
    @{ Name = 'feed stops both Emm motors'; Ok = $feedMain -match 'Emm_V5_Stop_Now\(1' -and $feedMain -match 'Emm_V5_Stop_Now\(3' },
    @{ Name = 'feed stops X motor'; Ok = $feedMain -match 'X_V5_Stop_Now\(2' },
    @{ Name = 'feed no longer uses touch edge flag'; Ok = $feedMain -notmatch 'TTP223_GetTouchFlag|TTP223_ClearTouchFlag' },
    @{ Name = 'feed PA6 has no EXTI configuration'; Ok = $feedTouch -notmatch 'EXTI_Init|NVIC_Init|EXTI9_5_IRQHandler' },
    @{ Name = 'descale polls PA6 level'; Ok = $descaleMain -match 'TTP223_ReadState\(\)' -and $descaleMain -notmatch 'TTP223_GetTouchFlag|TTP223_ClearTouchFlag' },
    @{ Name = 'descale stops all five X motors'; Ok = (1..5 | ForEach-Object { $descaleMain -match ("X_V5_Stop_Now\({0}" -f $_) }) -notcontains $false },
    @{ Name = 'descale stops both DC motors'; Ok = $descaleMain -match 'Motor1_SetSpeed\(0\)' -and $descaleMain -match 'Motor2_SetSpeed\(0\)' },
    @{ Name = 'descale PA6 has no EXTI configuration'; Ok = $descaleTouch -notmatch 'EXTI_Init|NVIC_Init|EXTI9_5_IRQHandler' },
    @{ Name = 'gutting polls PA6 level'; Ok = $gutMain -match 'TTP223_ReadState\(\)' -and $gutMain -notmatch 'TTP223_GetTouchFlag|TTP223_ClearTouchFlag' },
    @{ Name = 'gutting stops only X group and DC1'; Ok = (1..3 | ForEach-Object { $gutMain -match ("X_V5_Stop_Now\({0}" -f $_) }) -notcontains $false -and $gutMain -match 'Motor1_SetSpeed\(0\)' },
    @{ Name = 'gutting retains independent discharge receiver'; Ok = $gutMain -match 'MOTOR_START' -and $gutMain -match 'Emm_V5_Pos_Control\(1' },
    @{ Name = 'gutting PA6 has no EXTI configuration'; Ok = $gutTouch -notmatch 'EXTI_Init|NVIC_Init|EXTI9_5_IRQHandler' },
    @{ Name = 'discharge polls PA6 enable'; Ok = $dischargeMain -match 'TTP223_ReadState\(\)' },
    @{ Name = 'discharge low sends stop'; Ok = $dischargeMain -match 'MOTOR_STOP\\r\\n' },
    @{ Name = 'discharge start delay is cooperative'; Ok = $dischargeMain -match '20U' -and $dischargeMain -notmatch 'delay_ms\(2000\)' },
    @{ Name = 'discharge retains pressure threshold'; Ok = $dischargeMain -match 'pressure_voltage\s*<\s*0\.035f' },
    @{ Name = 'discharge PA6 has no EXTI configuration'; Ok = $dischargeTouch -notmatch 'EXTI_Init|NVIC_Init|EXTI9_5_IRQHandler' },
    @{ Name = 'gutting receiver handles discharge stop'; Ok = $gutMain -match 'MOTOR_STOP' -and $gutMain -match 'Emm_V5_Stop_Now\(1' },
    @{ Name = 'gutting receiver compares complete trimmed commands'; Ok = $gutMain -match 'Command_Equals\(rxCmd, rxCount' -and $gutMain -match "command\[length - 1U\] == '\\r'" }
)

$failed = @($checks | Where-Object { -not $_.Ok })
if ($failed.Count -gt 0) {
    foreach ($item in $failed) { Write-Output ("FAIL: " + $item.Name) }
    exit 1
}

Write-Output 'PASS: all four original-machine modules satisfy the PA6 level-control contract.'
