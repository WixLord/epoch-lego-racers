Add-Type -AssemblyName System.Drawing
$root = (Get-Location).Path
$bmp = [Drawing.Bitmap]::new((Join-Path $root 'Assets/Branding/LEGO-Racers-Epoch-Remastered.png'))
$rect = [Drawing.Rectangle]::new(0,0,$bmp.Width,$bmp.Height)
$data = $bmp.LockBits($rect,[Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
 $bytes = New-Object byte[] ($bmp.Width * $bmp.Height * 4)
 for ($y=0;$y -lt $bmp.Height;$y++) { [Runtime.InteropServices.Marshal]::Copy([IntPtr]::Add($data.Scan0,$y*$data.Stride),$bytes,$y*$bmp.Width*4,$bmp.Width*4) }
 $header = New-Object byte[] 18
 $header[2]=2
 [BitConverter]::GetBytes([UInt16]$bmp.Width).CopyTo($header,12)
 [BitConverter]::GetBytes([UInt16]$bmp.Height).CopyTo($header,14)
 $header[16]=32; $header[17]=40
 $stream=[IO.File]::Create((Join-Path $root 'Assets/Branding/RemasterLogo.tga'))
 try {$stream.Write($header,0,18);$stream.Write($bytes,0,$bytes.Length)} finally {$stream.Dispose()}
 Write-Output "Converted $($bmp.Width)x$($bmp.Height) RGBA logo to TGA"
} finally {$bmp.UnlockBits($data);$bmp.Dispose()}
Copy-Item Assets/Branding/RemasterLogo.tga 'Game Renderer Preview/RemasterLogo.tga'
