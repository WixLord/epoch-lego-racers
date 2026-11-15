$ErrorActionPreference='Stop'
Add-Type -Path @("$PSScriptRoot/BinaryTokens.cs","$PSScriptRoot/MeshAsset.cs")
$root=Join-Path $PSScriptRoot '../Assets/MotorbikeBuild'
foreach($dir in @('MENUDATA/PIECEDB','GAMEDATA/COMMON')){
 $mesh=[MeshAsset]::new((Join-Path $root "$dir/BIKE.GDB"))
 if($mesh.faces.Count -ne 564){throw 'Mesh missing'}
 foreach($node in 0..2){$count=@($mesh.faces | Where-Object node -eq $node).Count;Write-Output "$dir node $node : $count triangles"}
 $cmb=[BinaryTokens]::Read((Join-Path $root "$dir/CHASSIS.CMB"))
 $bikeIndex=-1;for($i=0;$i -lt $cmb.Count-1;$i++){if($cmb[$i].Type -eq 39 -and $cmb[$i+1].Value -eq 'bike'){$bikeIndex=$i;break}}
 if($bikeIndex -lt 0){throw 'Missing bike chassis'}
 # Physics must retain the donor's balanced footprint, mass and collision dimensions.
 foreach($section in @(@(42,3),@(44,1),@(46,2),@(49,14))){
  $token=$section[0];$length=$section[1]
  $donor=-1;$bike=-1
  for($i=5;$i -lt $bikeIndex;$i++){if($cmb[$i].Type -eq $token){$donor=$i;break}}
  for($i=$bikeIndex;$i -lt $cmb.Count;$i++){if($cmb[$i].Type -eq $token){$bike=$i;break}}
  if($donor -lt 0 -or $bike -lt 0){throw "Missing physics token $token"}
  for($j=1;$j -le $length;$j++){
   if($cmb[$donor+$j].Type -ne $cmb[$bike+$j].Type -or $cmb[$donor+$j].Value -ne $cmb[$bike+$j].Value){throw "Bike physics differs from donor at token $token offset $j"}
  }
 }
 Write-Output 'Donor mass, center of mass, collision dimensions and wheel contacts retained.'
 Write-Output "$dir chassis count: $($cmb[2].Value)"
 for($i=$bikeIndex;$i -lt $cmb.Count;$i++){if($cmb[$i].Type -eq 57){Write-Output "Shadow texture retained: $($cmb[$i+1].Value)"}}
}
foreach($name in @('LPIECEHI.LEB','LPIECELO.LEB')){
 $t=[BinaryTokens]::Read((Join-Path $root "MENUDATA/PIECEDB/$name"))
 $pieces=$t.FindIndex([Predicate[BinaryToken]]{param($x) $x.Type -eq 39})
 $shapes=$t.FindIndex([Predicate[BinaryToken]]{param($x) $x.Type -eq 44})
 $pos=$t.FindIndex([Predicate[BinaryToken]]{param($x) $x.Type -eq 41})
 $record=$t.FindIndex([Predicate[BinaryToken]]{param($x) $x.Type -eq 2 -and $x.Value -eq 'bike'})
 $shape=$shapes+5+2*[int]$t[$record+2].Value
 if($t[$shape].Value -ne 8 -or $t[$shape+1].Value -ne 1){throw 'Invalid bike grid'}
 $occupied=0;for($i=0;$i -lt 8;$i++){if(([int]$t[$shape+2+$i*2].Value -band 128) -ne 0){$occupied++}}
 if($occupied -ne 2){throw 'Bike must have exactly two attachment cells'}
 $position=[int]$t[$shape+18].Value+256*[int]$t[$shape+19].Value
 if($position -ge [int]$t[$pos+2].Value){throw 'Invalid grid offset'}
 $x=[int]$t[$pos+5+$position*3].Value/256.0
 $z=[int]$t[$pos+7+$position*3].Value/256.0
 if([Math]::Abs((.5-$x)-(-3.3469)) -gt .002 -or [Math]::Abs((9-$z)*.4-3.55) -gt .002){throw 'Backplate alignment incorrect'}
 Write-Output "$name : two usable rear studs aligned within 0.002 game units"
}

# Imported corner normals must survive serialization, including both hard and smooth edges.
$mesh=[MeshAsset]::new((Join-Path $PSScriptRoot '../Assets/Motorbike/BIKE.GDB'))
$smooth=0;$tread=@(0,0,0)
foreach($face in $mesh.faces){
 foreach($v in @($face.a,$face.b,$face.c)){
  if($null -eq $v.normal){throw 'Missing imported vertex normal'}
  $len=[Math]::Sqrt($v.normal[0]*$v.normal[0]+$v.normal[1]*$v.normal[1]+$v.normal[2]*$v.normal[2])
  if([Math]::Abs($len-1) -gt .002){throw 'Invalid vertex normal length'}
 }
 if(($face.a.normal -join ',') -ne ($face.b.normal -join ',')){$smooth++}
 if($face.mat -eq 4){$tread[$face.node]++;foreach($v in @($face.a,$face.b,$face.c)){if($v.a[3] -lt .519 -or $v.a[3] -gt .981 -or $v.a[4] -lt .509 -or $v.a[4] -gt .981){throw 'Tread UV outside atlas region'}}}
}
if($smooth -eq 0 -or $tread[0] -ne 0 -or $tread[1] -ne 24 -or $tread[2] -ne 24){throw 'Smoothing or tread assignment failed'}
foreach($dir in @('MENUDATA/PIECEDB','GAMEDATA/COMMON')){if(!(Test-Path (Join-Path $root "$dir/BB_TIRE.BMP"))){throw 'Missing tyre texture'}}
Write-Output 'Normal lengths, smooth faces, tread UVs and menu/race textures validated.'
