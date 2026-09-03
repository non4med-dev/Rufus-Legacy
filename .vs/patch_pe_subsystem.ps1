# This exists, because VS2022 refuses to accept /SUBVERSION:5.00 (Windows 2000) as a valid compiler flag; the lowest it supports is 5.01 (XP 32-bit)
# This script downgrades the PE header to 5.00 during compilation

param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][int]$Major,
    [Parameter(Mandatory = $true)][int]$Minor
)

$bytes = [System.IO.File]::ReadAllBytes($Path)
if (($bytes.Length -lt 0x100) -or ($bytes[0] -ne 0x4D) -or ($bytes[1] -ne 0x5A)) {
    throw "Target is not a valid DOS/PE image: $Path"
}

$peOffset = [System.BitConverter]::ToInt32($bytes, 0x3C)
if (($peOffset -lt 0) -or (($peOffset + 0x58) -gt $bytes.Length) -or
    ($bytes[$peOffset] -ne 0x50) -or ($bytes[$peOffset + 1] -ne 0x45) -or
    ($bytes[$peOffset + 2] -ne 0) -or ($bytes[$peOffset + 3] -ne 0)) {
    throw "Target contains an invalid PE header: $Path"
}

$optionalHeader = $peOffset + 24
$magic = [System.BitConverter]::ToUInt16($bytes, $optionalHeader)
if (($magic -ne 0x10B) -and ($magic -ne 0x20B)) {
    throw "Target contains an unsupported PE optional header: $Path"
}

# These subsystem-version fields have the same offsets in PE32 and PE32+. (port)
[System.Array]::Copy([System.BitConverter]::GetBytes([UInt16]$Major), 0, $bytes, $optionalHeader + 48, 2)
[System.Array]::Copy([System.BitConverter]::GetBytes([UInt16]$Minor), 0, $bytes, $optionalHeader + 50, 2)
[System.IO.File]::WriteAllBytes($Path, $bytes)
Write-Host "PE subsystem version set to $Major.$Minor for $Path"
