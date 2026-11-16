param(
    [string]$SourceArchive = "$PSScriptRoot/../Game Renderer Preview/LEGO.JAM",
    [string]$OutputDirectory = "$PSScriptRoot/../Assets/SpiderManBuild"
)
$ErrorActionPreference = 'Stop'
Add-Type -Path "$PSScriptRoot/BinaryTokens.cs"
Add-Type -Path "$PSScriptRoot/JamArchive.cs"
Add-Type -AssemblyName System.Drawing
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
& "$PSScriptRoot/Read-JamArchive.ps1" -Archive $SourceArchive -OutputDirectory $OutputDirectory | Out-Null
$parts = "$OutputDirectory/MENUDATA/PARTDB"
$art = [IO.Path]::GetFullPath("$PSScriptRoot/../Assets/SpiderMan")

# Ordinary bottom-up BMPs are supported by GolBmpFile. Keep the original UV
# layout: full front print on the right, color samples down the left margin.
$torso = New-Object Drawing.Bitmap(128,128,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [Drawing.Graphics]::FromImage($torso)
$g.Clear([Drawing.ColorTranslator]::FromHtml('#CE1726'))
$g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::Half
$front = [Drawing.Bitmap]::FromFile("$art/torso-front.png")
$g.DrawImage($front,[Drawing.Rectangle]::new(12,0,116,128))
$front.Dispose()
$blue = New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#1664BE'))
$g.FillRectangle($blue,0,74,10,32)
$g.Dispose()
$torso.Save("$parts/SP_CHST.BMP",[Drawing.Imaging.ImageFormat]::Bmp)
$torso.Save("$art/torso-game.png",[Drawing.Imaging.ImageFormat]::Png)
$torso.Dispose()
$legs = New-Object Drawing.Bitmap(16,16,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [Drawing.Graphics]::FromImage($legs); $g.Clear($blue.Color); $g.Dispose(); $blue.Dispose()
$legs.Save("$parts/SP_LEGS.BMP",[Drawing.Imaging.ImageFormat]::Bmp); $legs.Dispose()
$mask = [Drawing.Bitmap]::FromFile("$art/mask.png")
$face = New-Object Drawing.Bitmap(64,64,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [Drawing.Graphics]::FromImage($face); $g.DrawImageUnscaled($mask,0,0); $g.Dispose(); $mask.Dispose()
foreach($expression in @('DFLT','ANGRY','BLINK','HAPPY','SAD','SUPRZ')) {
    $face.Save("$parts/SP_$expression.BMP",[Drawing.Imaging.ImageFormat]::Bmp)
}
$face.Dispose()

# Append, never reorder: existing saved racers retain their cosmetic indices.
$catalog = [BinaryTokens]::Read("$parts/BODYPART.PCB")
foreach($part in @(@{Type=42;Name='sp_';Variant=$false},@{Type=43;Name='sp_chst';Variant=$true},@{Type=44;Name='sp_legs';Variant=$true})) {
    if($catalog | Where-Object {$_.Type -eq 2 -and $_.Value -eq $part.Name}) {throw 'Source already includes Spider-Man parts'}
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
    foreach($entry in @(@{From='lh_dflt';To='sp_dflt'},@{From='tu_chst';To='sp_chst'},@{From='leg_blue';To='sp_legs'})) {
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
[JamArchive]::Write($OutputDirectory,"$art/LEGO.JAM")
Write-Output "Built Spider-Man mask, torso and matching blue legs. Catalog indices: face 30, torso 29, legs 20."
