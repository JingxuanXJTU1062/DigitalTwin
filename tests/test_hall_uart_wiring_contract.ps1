$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$docFile = Get-ChildItem -LiteralPath $root -File -Filter '*5*.md' | Select-Object -First 1
if ($null -eq $docFile) { throw 'Pin-list markdown file was not found.' }
$doc = Get-Content -Raw -LiteralPath $docFile.FullName -Encoding UTF8

foreach ($required in @(
    'F407HALL PA9.*F103chuliao05 PA10',
    'F103chuliao05 PA9.*PD2',
    'F407HALL.*F103chuliao05.*PD2',
    '115200.*8-N-1',
    '12.*0x05.*16.*0x03'
)) {
    if ($doc -notmatch $required) {
        throw "Missing wiring/protocol documentation: $required"
    }
}
if ($doc -match 'F407HALL[^\r\n]*I2C3 从机' -or
    $doc -match 'PA8.*I2C3_SCL.*F407HALL' -or
    $doc -match 'PC9.*I2C3_SDA.*F407HALL') {
    throw 'Document still describes F407HALL-to-upper I2C wiring.'
}

Write-Host 'PASS: HALL-to-chuliao-to-upper UART wiring is documented consistently.'
