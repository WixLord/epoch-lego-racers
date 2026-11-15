$ErrorActionPreference='Stop'
Add-Type -Path @("$PSScriptRoot/BinaryTokens.cs","$PSScriptRoot/MeshAsset.cs","$PSScriptRoot/MeshBuilder.cs","$PSScriptRoot/ObjMeshImporter.cs","$PSScriptRoot/MotorbikeAssets.cs","$PSScriptRoot/JamArchive.cs")
$nodes=New-Object 'System.Collections.Generic.Dictionary[string,int]'
$nodes.Add('Frame',0);$nodes.Add('RearWheel',1);$nodes.Add('FrontWheel',2)
$mesh=[ObjMeshImporter]::Read((Join-Path $PSScriptRoot '../Exports/Cleaned-Motorbike/BikeChassis.obj'),$nodes,[double[][]]@(@(0,0,0),@(-2.7,0,1.3),@(2.7,0,1.3)))
if($mesh.faces.Count -ne 564){throw "Unexpected triangle count: $($mesh.faces.Count)"}
$meshPath=Join-Path $PSScriptRoot '../Assets/Motorbike/BIKE.GDB'
$mesh.Write($meshPath)
$check=[MeshAsset]::new($meshPath)
if($check.faces.Count -ne 564){throw 'Mesh roundtrip failed'}
Write-Output "Imported cleaned mesh: $($check.faces.Count) triangles"
[MotorbikeAssets]::Build((Join-Path $PSScriptRoot '../Assets/MotorbikeBuild'),$meshPath)
& "$PSScriptRoot/Build-MotorbikeAudio.ps1"
[JamArchive]::Write((Join-Path $PSScriptRoot '../Assets/MotorbikeBuild'),(Join-Path $PSScriptRoot '../Assets/Motorbike/LEGO.JAM'))
