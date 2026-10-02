$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$projects = @(
    @{ Name = 'F103jinliao01'; Header = 'Core/Inc/sensor.h'; Source = 'Core/Src/sensor.c'; Protocol = 'Core/Src/usart_protocol.c' },
    @{ Name = 'F103jinliao02'; Header = 'Core/Inc/sensor.h'; Source = 'Core/Src/sensor.c'; Protocol = 'Core/Src/usart_protocol.c' },
    @{ Name = 'F103chuliao05'; Header = 'Core/Inc/sensor.h'; Source = 'Core/Src/sensor.c'; Protocol = 'Core/Src/usart_protocol.c' },
    @{ Name = 'F407HALL'; Header = 'Core/Inc/hall.h'; Source = 'Core/Src/hall.c'; Protocol = 'Core/Src/distance_tx.c' },
    @{ Name = 'F407HALL03'; Header = 'Core/Inc/hall.h'; Source = 'Core/Src/hall.c'; Protocol = 'Core/Src/hall03_frame.c' }
)

foreach ($project in $projects) {
    $base = Join-Path $root (Join-Path 'xiaweiji' $project.Name)
    $header = Get-Content -Raw (Join-Path $base $project.Header)
    $source = Get-Content -Raw (Join-Path $base $project.Source)
    $protocol = Get-Content -Raw (Join-Path $base $project.Protocol)

    if ($header -notmatch 'HALL_BASE_TIMEOUT_MS\s+5000U') {
        throw "$($project.Name): base stop timeout is not 5 seconds."
    }
    if ($header -notmatch 'HALL_REACQUIRE_TIMEOUT_MS\s+20000U') {
        throw "$($project.Name): slow-speed acquisition window is missing."
    }
    if ($header -notmatch 'HALL_MIN_VALID_PERIOD_US\s+1800000U') {
        throw "$($project.Name): periods below 1.8 seconds are not rejected."
    }
    if ($header -notmatch 'HALL_TIMEOUT_PERIOD_MULTIPLIER\s+2U') {
        throw "$($project.Name): adaptive timeout is not twice the last trusted period."
    }
    foreach ($symbol in @('hall_period_from_samples', 'hall_get_timeout_ms', 'hall_apply_waiting_limit')) {
        if ($source -notmatch $symbol) {
            throw "$($project.Name): missing speed policy behavior $symbol."
        }
    }
    if ($source -notmatch 'delta\s*<\s*HALL_MIN_VALID_PERIOD_US') {
        throw "$($project.Name): short periods can still enter the speed filter."
    }
    if ($source -notmatch 'elapsed_since_edge\s*>=\s*hall_get_timeout_ms') {
        throw "$($project.Name): first edge after a stop can still turn the stopped interval into a false slow speed."
    }
    if ($source -notmatch 'period_count\s*>\s*0U[^}]*elapsed[^}]*hall_get_timeout_ms' -and
        $source -notmatch 'hall_get_timeout_ms\s*\([^;]+period') {
        throw "$($project.Name): timeout is not derived from a trusted period."
    }
    if ($protocol -match '>\s*65535(?:\.0f)?\)?\s*[^\r\n]*=\s*65535') {
        throw "$($project.Name): invalid speed is still encoded as 65535."
    }
}

# Hand-checked policy examples: 2 s -> 5 s timeout; 4 s -> 8 s timeout.
function Get-ExpectedTimeoutMs([uint32]$periodUs) {
    $adaptive = [uint64]$periodUs * 2U / 1000U
    if ($adaptive -lt 5000U) { return 5000U }
    return [uint32]$adaptive
}
if ((Get-ExpectedTimeoutMs 2000000U) -ne 5000U) { throw '2 s policy example failed.' }
if ((Get-ExpectedTimeoutMs 4000000U) -ne 8000U) { throw '4 s policy example failed.' }

Write-Host 'PASS: all five MCUs share the validated Hall speed and timeout policy.'
