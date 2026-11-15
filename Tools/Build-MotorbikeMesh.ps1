$ErrorActionPreference='Stop'
Add-Type -Path "$PSScriptRoot/BinaryTokens.cs","$PSScriptRoot/MeshAsset.cs","$PSScriptRoot/MeshBuilder.cs"
$builder=[MeshBuilder]::new()
$gray=0xb8bbb5
# Classic frame: rear stud platform, low seat, rising tank, and diagonal front forks.
$builder.Extrude(@(@(-3.05,2.6),@(-1.2,2.6),@(-.45,1.65),@(.3,1.65),@(.85,3.05),@(.55,3.55),@(-.35,3.55),@(-1,2.95),@(-3.05,3.55)),.52,$gray)
$builder.Extrude(@(@(.25,2.05),@(1.25,2.15),@(1.9,3.45),@(1.55,3.75),@(.85,3.1)),.48,$gray)
$builder.Cylinder(@(-2.7,0,1.3),@(-2.2,0,2.9),.23,6,$gray)
foreach($y in @(-.6,.6)){
 $builder.Cylinder(@(-2.8,$y,1.05),@(.2,$y,1.05),.18,6,$gray)
 $builder.Cylinder(@(.2,$y,1.05),@(1.25,$y,2.3),.18,6,$gray)
 $builder.Cylinder(@(2.7,$y,1.3),@(1.65,$y,3.65),.22,6,$gray)
}
$builder.Cylinder(@(1.55,0,3.4),@(1.2,0,4.15),.22,8,$gray)
$builder.Cylinder(@(1.2,-1.05,4.15),@(1.2,1.05,4.15),.16,8,$gray)
$builder.Cylinder(@(1.85,0,3.25),@(2.15,0,3.25),.38,8,$gray)
$builder.Cylinder(@(2.16,0,3.25),@(2.19,0,3.25),.24,8,0x555952)
foreach($x in @(-2.55,-1.55)){$builder.Cylinder(@($x,0,3.55),@($x,0,3.76),.28,8,$gray)}
# Two independently animated wheels. Node origins are the axle centres.
foreach($node in @(1,2)){
 $builder.Cylinder(@(0,-.35,0),@(0,.35,0),1.3,12,0x202020,$node)
 $builder.Cylinder(@(0,-.37,0),@(0,.37,0),.68,12,0xa41e20,$node)
}
$output=Join-Path $PSScriptRoot '../Assets/Motorbike/BIKE.GDB'
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($output))|Out-Null
$builder.mesh.Write($output)
Write-Output "Motorbike: $($builder.mesh.faces.Count) triangles"
