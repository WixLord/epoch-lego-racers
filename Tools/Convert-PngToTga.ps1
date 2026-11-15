param([Parameter(Mandatory=$true)][string]$Source, [Parameter(Mandatory=$true)][string]$Destination, [ValidateRange(0,65535)][int]$MaxWidth = 0)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Source).Path)
try {
    if ($MaxWidth -gt 0 -and $bitmap.Width -gt $MaxWidth) {
        $height = [Math]::Max(1, [int][Math]::Round($bitmap.Height * $MaxWidth / $bitmap.Width))
        $resized = [System.Drawing.Bitmap]::new($MaxWidth, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($resized)
        try {
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($bitmap, [System.Drawing.Rectangle]::new(0, 0, $MaxWidth, $height))
        } finally { $graphics.Dispose() }
        $bitmap.Dispose()
        $bitmap = $resized
    }
    if ($bitmap.Width -gt 65535 -or $bitmap.Height -gt 65535) { throw 'Image exceeds TGA limits' }
    $rect = [System.Drawing.Rectangle]::new(0, 0, $bitmap.Width, $bitmap.Height)
    $locked = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $stream = [System.IO.File]::Create($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Destination))
        try {
            $header = [byte[]]::new(18)
            $header[2] = 2 # uncompressed true-color
            [BitConverter]::GetBytes([uint16]$bitmap.Width).CopyTo($header,12)
            [BitConverter]::GetBytes([uint16]$bitmap.Height).CopyTo($header,14)
            $header[16] = 32
            $header[17] = 8 # bottom origin, 8 alpha bits; the legacy loader assumes bottom-up rows
            $stream.Write($header,0,18)
            $row = [byte[]]::new($bitmap.Width * 4)
            for ($y = $bitmap.Height - 1; $y -ge 0; $y--) {
                [Runtime.InteropServices.Marshal]::Copy([IntPtr]::Add($locked.Scan0, $y * $locked.Stride), $row, 0, $row.Length)
                $stream.Write($row,0,$row.Length)
            }
        } finally { $stream.Dispose() }
    } finally { $bitmap.UnlockBits($locked) }
    Write-Output "Exported $($bitmap.Width)x$($bitmap.Height) RGBA pixels to bottom-origin TGA."
} finally { $bitmap.Dispose() }
