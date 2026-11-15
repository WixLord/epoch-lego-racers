param(
    [string]$SourceDirectory = "$PSScriptRoot/../Assets/MotorbikeBuild",
    [string]$OutputDirectory = "$PSScriptRoot/../Assets/DriverDetail"
)
$ErrorActionPreference = 'Stop'
Add-Type -Path "$PSScriptRoot/BinaryTokens.cs"
$data = @{}
foreach ($variant in @('RR','RP','HR','HP')) {
    foreach ($extension in @('GDB','SDB')) {
        $data["menu/$variant.$extension"] = [BinaryTokens]::Read("$SourceDirectory/MENUDATA/PARTDB/MENUPART/$variant.$extension")
    }
}
foreach ($extension in @('GDB','SDB')) {
    $data["low/LEG_BOX.$extension"] = [BinaryTokens]::Read("$SourceDirectory/MENUDATA/PARTDB/GAMEPART/LEG_BOX.$extension")
}
$files = @{
    'race/PELVIS.SDB' = 'GAMEDATA/COMMON/PELVIS.SDB'
    catalog = 'MENUDATA/PARTDB/BODYPART.PCB'
    materials = 'MENUDATA/PARTDB/BODYPART.MDB'
    textures = 'MENUDATA/PARTDB/BODYPART.TDB'
    hats = 'MENUDATA/PARTDB/MENUPART/CBBODIES.GCB'
    drivers = 'GAMEDATA/COMMON/DRIVERS.DDB'
}
foreach ($key in $files.Keys) { $data[$key] = [BinaryTokens]::Read((Join-Path $SourceDirectory $files[$key])) }
$inputFile = "$PSScriptRoot/../Build/driver-detail-input.json"
$data | ConvertTo-Json -Depth 5 -Compress | Set-Content $inputFile
& 'C:/Program Files/Autodesk/3ds Max 2023/Python/python.exe' "$PSScriptRoot/Build-DriverDetail.py" $inputFile $OutputDirectory $SourceDirectory
if ($LASTEXITCODE) { throw 'Driver detail conversion failed' }
# Custom drivers already merge the selected hat through this resource library.
Copy-Item "$SourceDirectory/MENUDATA/PARTDB/MENUPART/CBBODIES.GCB" "$OutputDirectory/MENUDATA/PARTDB/GAMEPART/ICB_CHAR.GCB" -Force
