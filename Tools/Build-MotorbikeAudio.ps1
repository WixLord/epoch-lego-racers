$ErrorActionPreference='Stop'
# Encode against the game's actual ADPCM step table (high nibble first).
$source=Get-Content "$PSScriptRoot/../Source/racers/LEGORacers/src/audio/audiohelpers.cpp" -Raw
$table=[regex]::Match($source,'g_adpcmStepTable\[89\]\s*=\s*\{([^}]+)\}').Groups[1].Value
$steps=[int[]]([regex]::Matches($table,'\d+') | ForEach-Object {$_.Value})
$adjust=@(-1,-1,-1,-1,2,4,6,8)
$folder=Join-Path $PSScriptRoot '../Assets/MotorbikeBuild/GAMEDATA/COMMON'
foreach($mode in 0..2){
 # Race playback pitches the driving loop down to about half its source frequency.
 $n=22048; $frequency=@(110,300,400)[$mode]; $samples=New-Object int[] $n
 for($i=0;$i -lt $n;$i++){
  $phase=2*[math]::PI*[math]::Round($frequency*$n/11025)*$i/$n
  $tone=0.0
  for($h=1;$h -le 16;$h++){$tone+=[math]::Sin($phase*$h)/[math]::Pow($h,1.0)}
  $envelope=.82+.18*[math]::Sin(2*[math]::PI*6*$i/$n)
  $fade=[math]::Min(1,[math]::Min($i,($n-1-$i))/60)
  $samples[$i]=[int](12000*$tone*$envelope*$fade)
 }
 $encoded=New-Object byte[] ($n/2); $prediction=0; $index=0; $maxError=0
 for($i=0;$i -lt $n;$i++){
  $diff=$samples[$i]-$prediction; $step=$steps[$index]
  $magnitude=[math]::Min(7,[int][math]::Round([math]::Abs($diff)*4/$step))
  $nibble=$magnitude; if($diff -lt 0){$nibble=$nibble -bor 8}
  $delta=($magnitude*$step) -shr 2; if($diff -lt 0){$delta=-$delta}
  $prediction=[math]::Max(-32768,[math]::Min(32767,$prediction+$delta))
  $index=[math]::Max(0,[math]::Min(88,$index+$adjust[$magnitude]))
  $byteIndex=$i -shr 1
  if(($i -band 1) -eq 0){$encoded[$byteIndex]=$nibble -shl 4}else{$encoded[$byteIndex]=$encoded[$byteIndex] -bor $nibble}
  $maxError=[math]::Max($maxError,[math]::Abs($samples[$i]-$prediction))
 }
 [IO.File]::WriteAllBytes((Join-Path $folder "BIKE$mode.PCM"),$encoded)
 Write-Output "Bike audio $mode : $n samples, peak encoding error $maxError"
}
$bank=Join-Path $folder 'GENERAL.SBK'; $lines=[IO.File]::ReadAllLines($bank)
if([int]$lines[1] -ne 80){throw 'Expected original 80-entry sound bank'}
$lines[1]='83'; $lines+=@('bike0.pcm','bike1.pcm','bike2.pcm')
[IO.File]::WriteAllLines($bank,$lines,[Text.Encoding]::ASCII)
