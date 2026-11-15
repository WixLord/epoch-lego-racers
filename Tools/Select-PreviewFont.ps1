param([string]$Font)
$ErrorActionPreference = 'Stop'
$game = Join-Path (Split-Path $PSScriptRoot -Parent) 'Game Renderer Preview'
$folder = Join-Path $game 'Fonts'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$files = @(Get-ChildItem -LiteralPath $folder -File | Where-Object Extension -in '.otf','.ttf' | Sort-Object Name)
if (!$Font) {
    Write-Host "LEGO Racers - choose a font`n"
    Write-Host '0. Original LEGO Racers lettering'
    for ($i=0; $i -lt $files.Count; $i++) { Write-Host ("{0}. {1}" -f ($i+1),$files[$i].BaseName) }
    Write-Host "A. Add another .ttf or .otf file`n"
    $choice = Read-Host 'Choose a number, or A'
    if ($choice -eq 'A') {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = [Windows.Forms.OpenFileDialog]::new()
        $dialog.Title = 'Choose a font for your personal game preview'
        $dialog.Filter = 'Font files (*.ttf;*.otf)|*.ttf;*.otf'
        if ($dialog.ShowDialog() -ne 'OK') { $dialog.Dispose(); return }
        $Font = $dialog.FileName
        $dialog.Dispose()
    } elseif ($choice -eq '0') { $Font='original' }
    else {
        $number=0
        if (![int]::TryParse($choice,[ref]$number) -or $number -lt 1 -or $number -gt $files.Count) { throw 'Invalid selection.' }
        $Font=$files[$number-1].FullName
    }
}
if ($Font -eq 'original') { $setting='original' }
else {
    if (!(Test-Path -LiteralPath $Font -PathType Leaf)) {
        $match=@($files | Where-Object { $_.BaseName -eq $Font -or $_.Name -eq $Font })
        if ($match.Count -ne 1) { throw 'Font not found. Supply an existing .ttf or .otf file.' }
        $Font=$match[0].FullName
    }
    $source=(Resolve-Path -LiteralPath $Font).Path
    if ([IO.Path]::GetExtension($source) -notin '.ttf','.otf') { throw 'Choose a .ttf or .otf file.' }
    Add-Type -AssemblyName System.Drawing
    $collection=[Drawing.Text.PrivateFontCollection]::new()
    try {
        $collection.AddFontFile($source)
        if (!$collection.Families.Count) { throw 'This font could not be loaded.' }
    } finally { $collection.Dispose() }
    $destination=Join-Path $folder ([IO.Path]::GetFileName($source))
    if ($source -ne $destination) {
        if (Test-Path -LiteralPath $destination) { throw 'A font with that filename already exists. Rename the new file first.' }
        Copy-Item -LiteralPath $source -Destination $destination
    }
    $setting='Fonts\'+[IO.Path]::GetFileName($destination)
}
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class IniWriter {
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 [return: MarshalAs(UnmanagedType.Bool)]
 public static extern bool WritePrivateProfileString(string section,string key,string value,string filename);
}
'@
if (![IniWriter]::WritePrivateProfileString('UI','FontFile',$setting,(Join-Path $game 'Reborn.ini'))) { throw 'Could not save the font selection.' }
Write-Host "`nSelected: $setting"
Write-Host 'Restart the preview game to apply it. No rebuild or Windows font installation needed.'
