param(
    [string]$SourceArchive = "$PSScriptRoot/../Build/BatmanBase-v0.1.8.JAM",
    [string]$OutputDirectory = "$PSScriptRoot/../Assets/BatmanBuild"
)
$ErrorActionPreference = 'Stop'
Add-Type -Path "$PSScriptRoot/BinaryTokens.cs"
Add-Type -Path "$PSScriptRoot/JamArchive.cs"
Add-Type -AssemblyName System.Drawing
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
& "$PSScriptRoot/Read-JamArchive.ps1" -Archive $SourceArchive -OutputDirectory $OutputDirectory | Out-Null
$parts = "$OutputDirectory/MENUDATA/PARTDB"
$art = [IO.Path]::GetFullPath("$PSScriptRoot/../Assets/Batman")

# Ordinary bottom-up BMPs are supported by GolBmpFile. Keep the original UV
# layout: full front print on the right, color samples down the left margin.
$torso = New-Object Drawing.Bitmap(128,128,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [Drawing.Graphics]::FromImage($torso)
$g.Clear([Drawing.ColorTranslator]::FromHtml('#858A94'))
$g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::Half
# Preserve the user's edited artwork independently of the procedural paint source.
$front = [Drawing.Bitmap]::FromFile("$art/torso-front-custom.png")
$g.DrawImage($front,[Drawing.Rectangle]::new(12,0,116,128))
$front.Dispose()
$gloves = New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#171923'))
$g.FillRectangle($gloves,0,32,10,24)
$g.FillRectangle($gloves,0,74,10,32)
$gloves.Dispose()
$blue = New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#171923'))
$g.FillRectangle($blue,0,74,10,32)
$g.Dispose()
$torso.Save("$parts/BT_CHST.BMP",[Drawing.Imaging.ImageFormat]::Bmp)
$torso.Save("$art/torso-game.png",[Drawing.Imaging.ImageFormat]::Png)
$torso.Dispose()
$legs = New-Object Drawing.Bitmap(16,16,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [Drawing.Graphics]::FromImage($legs); $g.Clear($blue.Color); $g.Dispose(); $blue.Dispose()
$legs.Save("$parts/BT_LEGS.BMP",[Drawing.Imaging.ImageFormat]::Bmp); $legs.Dispose()
$mask = [Drawing.Bitmap]::FromFile("$art/mask.png")
$face = New-Object Drawing.Bitmap(64,64,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [Drawing.Graphics]::FromImage($face); $g.DrawImageUnscaled($mask,0,0); $g.Dispose(); $mask.Dispose()
foreach($expression in @('DFLT','ANGRY','BLINK','HAPPY','SAD','SUPRZ')) {
    $face.Save("$parts/BT_$expression.BMP",[Drawing.Imaging.ImageFormat]::Bmp)
}
$face.Dispose()

# Append, never reorder: existing saved racers retain their cosmetic indices.
$catalog = [BinaryTokens]::Read("$parts/BODYPART.PCB")
foreach($part in @(@{Type=42;Name='bt_';Variant=$false},@{Type=43;Name='bt_chst';Variant=$true},@{Type=44;Name='bt_legs';Variant=$true})) {
    if($catalog | Where-Object {$_.Type -eq 2 -and $_.Value -eq $part.Name}) {throw 'Source already includes Batman parts'}
    $start = 0
    while($catalog[$start].Type -ne $part.Type){$start++}
    $catalog[$start+2].Value = [int]$catalog[$start+2].Value + 1
    $end = $start+5
    while($catalog[$end].Type -ne 6){$end++}
    $catalog.Insert($end++,[BinaryToken]::new(2,$part.Name))
    if($part.Variant){$catalog.Insert($end++,[BinaryToken]::new(4,0))}
    $catalog.Insert($end,[BinaryToken]::new(4,0))
}
[BinaryTokens]::Write("$parts/BODYPART.PCB",$catalog)

foreach($file in @('BODYPART.MDB','BODYPART.TDB')) {
    $tokens = [BinaryTokens]::Read("$parts/$file")
    if ($file -eq 'BODYPART.MDB') {
        # A dedicated dark material avoids changing the original game's black hats.
        $tokens.Insert($tokens.Count-1,[BinaryToken]::new(39))
        $tokens.Insert($tokens.Count-1,[BinaryToken]::new(2,'bt_cowl'))
        $tokens.Insert($tokens.Count-1,[BinaryToken]::new(5))
        foreach ($kind in @(41,40)) {
            $tokens.Insert($tokens.Count-1,[BinaryToken]::new($kind))
            foreach ($channel in @(18,19,23,255)) {
                $tokens.Insert($tokens.Count-1,[BinaryToken]::new(4,$channel))
            }
        }
        $tokens.Insert($tokens.Count-1,[BinaryToken]::new(6))
        $tokens[2].Value=[int]$tokens[2].Value+1
    }
    foreach($entry in @(@{From='lh_dflt';To='bt_dflt'},@{From='tu_chst';To='bt_chst'},@{From='leg_blue';To='bt_legs'})) {
        $start=5
        while(!($tokens[$start].Type -eq 39 -and $tokens[$start+1].Value -eq $entry.From)){$start++}
        $end=$start+3
        while($tokens[$end].Type -ne 6){$end++}
        $dest=$tokens.Count-1
        while($tokens[$dest].Type -ne 6){$dest--}
        for($i=$start;$i -le $end;$i++) {
            $value=$tokens[$i].Value
            if($tokens[$i].Type -eq 2 -and $value -eq $entry.From){$value=$entry.To}
            $tokens.Insert($dest++,[BinaryToken]::new($tokens[$i].Type,$value))
        }
        $tokens[2].Value=[int]$tokens[2].Value+1
    }
    [BinaryTokens]::Write("$parts/$file",$tokens)
}
# Saved-car texture definitions retain bare filenames. Race and cinematic
# reloads resolve these from their own resource directories, not PARTDB.
# Mirror the original default face's resource coverage for every new part.
$resourceDirectories = Get-ChildItem $OutputDirectory -Recurse -Filter 'LH_*.BMP' |
    Select-Object -ExpandProperty DirectoryName -Unique
foreach($directory in $resourceDirectories) {
    if([IO.Path]::GetFullPath($directory) -eq [IO.Path]::GetFullPath($parts)){continue}
    foreach($name in @('BT_DFLT','BT_ANGRY','BT_BLINK','BT_HAPPY','BT_SAD','BT_SUPRZ','BT_CHST','BT_LEGS')) {
        Copy-Item "$parts/$name.BMP" "$directory/$name.BMP" -Force
    }
}
$cowlInput = @{}
foreach($file in @('MENUPART/CBBODIES.GCB','GAMEPART/ICB_CHAR.GCB','BODYPART.PCB')) {
    $cowlInput[$file] = [BinaryTokens]::Read("$parts/$file")
}
$cowlInput | ConvertTo-Json -Depth 5 -Compress | Set-Content "$PSScriptRoot/../Build/bat-hats.json"
Push-Location "$PSScriptRoot/.."
try {
    & 'Build/network-venv/Scripts/python.exe' 'Tools/Build-BatmanCowl.py'
    if($LASTEXITCODE){throw 'Batman cowl conversion failed'}
} finally { Pop-Location }
$bodyInput = @{}
foreach($directory in @('MENUPART','GAMEPART')) {
    foreach($name in @('RR','RP','HR','HP','LEG_BOX','BIKEMAN','CHESTS')) {
        $file = "$parts/$directory/$name.GDB"
        if(Test-Path $file){$bodyInput["$directory/$name.GDB"]=[BinaryTokens]::Read($file)}
    }
}
$bodyInput | ConvertTo-Json -Depth 5 -Compress | Set-Content "$PSScriptRoot/../Build/bat-bodies.json"
[BinaryTokens]::Read("$parts/MENUPART/SELECTOR.WDB") | ConvertTo-Json -Depth 5 -Compress | Set-Content "$PSScriptRoot/../Build/bat-selector.json"
Push-Location "$PSScriptRoot/.."
try {
    & 'Build/network-venv/Scripts/python.exe' 'Tools/Build-CapeBodies.py'
    if($LASTEXITCODE){throw 'Cape conversion failed'}
} finally { Pop-Location }
[JamArchive]::Write($OutputDirectory,"$art/LEGO.JAM")
Write-Output "Built Batman mask, torso and black legs and cowl ears. Catalog indices: face 31, torso 30, legs 21."
