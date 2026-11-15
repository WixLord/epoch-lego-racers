param([string]$Archive='Game Renderer Preview/LEGO.JAM',[string]$Pattern='*',[string]$OutputDirectory='')
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Archive))
if([Text.Encoding]::ASCII.GetString($bytes,0,4) -ne 'LJAM'){throw 'Invalid archive signature'}
function Read-DirectoryEntries([int]$Offset,[string]$Prefix) {
 $count=[BitConverter]::ToUInt32($bytes,$Offset);$Offset+=4
 for($i=0;$i -lt $count;$i++) {
  $name=[Text.Encoding]::ASCII.GetString($bytes,$Offset,12).TrimEnd([char]0)
  $position=[BitConverter]::ToUInt32($bytes,$Offset+12);$size=[BitConverter]::ToUInt32($bytes,$Offset+16);$Offset+=20
  $path=$Prefix+$name
  if($path -like $Pattern){
   if($OutputDirectory){$root=[IO.Path]::GetFullPath($OutputDirectory);$target=[IO.Path]::GetFullPath((Join-Path $root $path));if(!$target.StartsWith($root+[IO.Path]::DirectorySeparatorChar)){throw 'Invalid archive path'};[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null; $data=New-Object byte[] $size;[Array]::Copy($bytes,$position,$data,0,$size);[IO.File]::WriteAllBytes($target,$data)}
   [pscustomobject]@{Path=$path;Offset=$position;Size=$size}
  }
 }
 $count=[BitConverter]::ToUInt32($bytes,$Offset);$Offset+=4
 for($i=0;$i -lt $count;$i++){$name=[Text.Encoding]::ASCII.GetString($bytes,$Offset,12).TrimEnd([char]0);$child=[BitConverter]::ToUInt32($bytes,$Offset+12);$Offset+=16;Read-DirectoryEntries $child ($Prefix+$name+'/')}
}
Read-DirectoryEntries 4 ''
