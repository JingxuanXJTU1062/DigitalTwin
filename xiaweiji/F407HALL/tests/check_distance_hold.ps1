$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$main = Get-Content -Raw (Join-Path $root 'Core/Src/main.c')
$driver = Get-Content -Raw (Join-Path $root 'Core/Src/vl6180x.c')

if ($main -match '\*distance_mm\s*=\s*0U') {
    throw 'Application clears the last valid distance before a measurement succeeds.'
}
if ($main -notmatch '\*distance_mm\s*=\s*distance;') {
    throw 'Application does not latch distance after a successful measurement.'
}
if ($driver -notmatch 'uint8_t\s+measured_distance') {
    throw 'Driver has no temporary measurement value.'
}
if ($driver -match '\*distance_mm\s*=\s*0U') {
    throw 'Driver clears the caller distance on entry.'
}
if ($driver -notmatch '\*distance_mm\s*=\s*measured_distance;') {
    throw 'Driver does not publish distance only after full success.'
}

Write-Host 'PASS: failed measurements preserve the last successful distance.'
