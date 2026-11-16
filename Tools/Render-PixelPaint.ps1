param([string]$Directory = "$PSScriptRoot/../Assets/SpiderMan")
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
foreach ($name in @('mask','torso-front','torso-back')) {
    $data = Get-Content "$Directory/$name.json" -Raw | ConvertFrom-Json
    # Supersample the authored shapes, then retain the original game resolution.
    # This gives thin prints edge coverage like the original painted textures.
    $scale = 4
    $bitmap = New-Object Drawing.Bitmap(([int]$data.width*$scale), ([int]$data.height*$scale))
    $g = [Drawing.Graphics]::FromImage($bitmap)
    $g.ScaleTransform($scale,$scale)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::Half
    foreach ($layer in $data.layers) {
        foreach ($shape in $layer.shapes) {
            $points = [Drawing.PointF[]]@($shape.points | ForEach-Object { New-Object Drawing.PointF([single]$_[0], [single]$_[1]) })
            $brush = New-Object Drawing.SolidBrush([Drawing.ColorTranslator]::FromHtml('#' + $shape.color))
            $g.FillPolygon($brush,$points)
            $brush.Dispose()
        }
    }
    $g.Dispose()
    $result = New-Object Drawing.Bitmap([int]$data.width, [int]$data.height)
    $resize = [Drawing.Graphics]::FromImage($result)
    $resize.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $resize.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::Half
    $attributes = New-Object Drawing.Imaging.ImageAttributes
    $attributes.SetWrapMode([Drawing.Drawing2D.WrapMode]::TileFlipXY)
    $resize.DrawImage($bitmap,[Drawing.Rectangle]::new(0,0,$result.Width,$result.Height),0,0,$bitmap.Width,$bitmap.Height,[Drawing.GraphicsUnit]::Pixel,$attributes)
    $attributes.Dispose()
    $resize.Dispose()
    $result.Save("$Directory/$name.png",[Drawing.Imaging.ImageFormat]::Png)
    $result.Dispose()
    $bitmap.Dispose()
}
