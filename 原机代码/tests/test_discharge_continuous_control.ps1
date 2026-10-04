$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$gutting = Get-Content -Raw (Join-Path $root '4_开膛去内脏\APP\main.c')
$discharge = Get-Content -Raw (Join-Path $root '5_出料\APP\main.c')
$guttingStop = [regex]::Match(
    $gutting,
    'static void Gutting_Stop\(void\)\s*\{(?<body>[\s\S]*?)\}\s*static uint8_t Gutting_Continue'
).Groups['body'].Value

if ([string]::IsNullOrWhiteSpace($guttingStop)) {
    throw 'Could not isolate Gutting_Stop for ownership verification.'
}
if ($guttingStop -match 'Emm_V5_Stop_Now\(1, 0\);') {
    throw 'Flag3 Gutting_Stop must not stop the Flag4-owned discharge Emm #1.'
}
if ($gutting -notmatch 'Command_Equals\(rxCmd, rxCount, "MOTOR_RUN"\)[\s\S]*?Emm_V5_Vel_Control\(1, 1, 100, 0, 0\);') {
    throw 'MOTOR_RUN must select continuous velocity control.'
}
if ($gutting -notmatch 'Command_Equals\(rxCmd, rxCount, "MOTOR_START"\)[\s\S]*?Emm_V5_Pos_Control\(1, 1, 100, 0, 25000, 2, 0\);') {
    throw 'Pressure MOTOR_START must select finite 25000-pulse position control.'
}
if ($gutting -notmatch 'Command_Equals\(rxCmd, rxCount, "MOTOR_STOP"\)[\s\S]*?Emm_V5_Stop_Now\(1, 0\);') {
    throw 'MOTOR_STOP must immediately stop discharge Emm #1.'
}
if ($discharge -notmatch 'printf\("MOTOR_RUN\\r\\n"\)' -or
    $discharge -notmatch 'printf\("MOTOR_START\\r\\n"\)' -or
    $discharge -notmatch 'printf\("MOTOR_STOP\\r\\n"\)') {
    throw 'Discharge board must own all three command decisions.'
}
if ($discharge -notmatch 'if \(pressure_voltage < PRESSURE_TRIGGER_VOLTAGE\)\s*\{\s*pressure_armed = 1U;\s*pressure_active = 0U;\s*start_delay_ticks = 0U;' -or
    $discharge -notmatch 'else if \(pressure_armed\)\s*\{[\s\S]*?pressure_event = 1U;') {
    throw 'Pressure must trigger at or above 0.035 V; low voltage must re-arm without starting.'
}
if ($discharge -notmatch '#define PRESSURE_TRIGGER_ENABLED\s+1U' -or
    $discharge -notmatch '#if PRESSURE_TRIGGER_ENABLED[\s\S]*?PressureSensor_Init\(\);[\s\S]*?#endif' -or
    $discharge -notmatch '#if PRESSURE_TRIGGER_ENABLED[\s\S]*?pressure_event[\s\S]*?#endif') {
    throw 'Pressure trigger code must be enabled through its compile-time switch.'
}

Write-Host 'PASS: Flag4 continuous, pressure finite, and Flag3 isolated.'
