param([string]$SourceArchive="$PSScriptRoot/../Build/PlatesBase-v0.1.9.JAM")
$ErrorActionPreference='Stop'
Add-Type -Path @("$PSScriptRoot/BinaryTokens.cs","$PSScriptRoot/MeshAsset.cs","$PSScriptRoot/JamArchive.cs")
$root=[IO.Path]::GetFullPath("$PSScriptRoot/../Assets/PlatesBuild")
& "$PSScriptRoot/Read-JamArchive.ps1" -Archive $SourceArchive -OutputDirectory $root | Out-Null
$inputData=@{}
foreach($f in Get-ChildItem $root -Recurse -Filter 'LPIECE*.LEB') { $inputData[$f.FullName.Substring($root.Length+1).Replace('\','/')]=[BinaryTokens]::Read($f.FullName) }
foreach($name in @('DP_CSET.LEB','L_COLORS.LEB')){$inputData["MENUDATA/PIECEDB/$name"]=[BinaryTokens]::Read("$root/MENUDATA/PIECEDB/$name")}
$inputData | ConvertTo-Json -Depth 5 -Compress | Set-Content "$PSScriptRoot/../Build/plates-input.json"
Push-Location "$PSScriptRoot/.."
try { & Build/network-venv/Scripts/python.exe Tools/Build-Plates.py; if($LASTEXITCODE){throw 'Plate conversion failed'} } finally {Pop-Location}
foreach($f in Get-ChildItem $root -Recurse -Filter '*.GDB') {
 if($f.BaseName -match 'CM$') {
  $mesh=[MeshAsset]::new($f.FullName)
  $count=$mesh.AddStuds()
  if($count){$mesh.Write($f.FullName);Write-Output "AI studs: $($f.BaseName), $count studs"}
 }
}
[JamArchive]::Write($root,"$PSScriptRoot/../Assets/Plates/LEGO.JAM")
