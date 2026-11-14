<#
.SYNOPSIS
  Draws the EcoWattSense icon (variant C: meter dial with a leaf) and writes assets\ecowattsense.ico and assets\ecowattsense-256.png.
.DESCRIPTION
  Who uses it: a developer, once, when the drawing changes. The two output files are committed; the build only reads the .ico.
  The drawing is the 100x100 design of icon-c.svg, redrawn with WPF (DrawingVisual, RenderTargetBitmap, PngBitmapEncoder),
  so no extra tool is needed. Sizes 16, 20 and 24 px use a simpler form for readability: no leaf, strokes 1.5 times thicker.
  The .ico holds PNG frames (supported by Windows Vista and later).
  Run: pwsh -NoProfile -ExecutionPolicy Bypass -File New-AppIcon.ps1
.PARAMETER OutDir
  The folder that receives the files. Default: ..\assets next to this script.
#>
param([string] $OutDir = (Join-Path $PSScriptRoot '..\assets'))

Add-Type -AssemblyName PresentationCore, WindowsBase

$Sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
$SmallLimit = 24

<# Makes a frozen brush from a hex colour. #>
function New-Brush([string] $Hex) {
    $brush = New-Object System.Windows.Media.SolidColorBrush ([System.Windows.Media.ColorConverter]::ConvertFromString($Hex))
    $brush.Freeze()
    return $brush
}

<# Makes a round-capped pen. #>
function New-Pen([string] $Hex, [double] $Width) {
    $pen = New-Object System.Windows.Media.Pen (New-Brush $Hex), $Width
    $pen.StartLineCap = 'Round'
    $pen.EndLineCap = 'Round'
    $pen.Freeze()
    return $pen
}

<#
  Draws the icon at one size and returns the PNG bytes.
  Takes the pixel size. The small form is used up to $SmallLimit px. Nothing can fail except a missing WPF assembly.
#>
function New-IconPng([int] $Size) {
    $small = $Size -le $SmallLimit
    $thick = if ($small) { 1.5 } else { 1.0 }
    $visual = New-Object System.Windows.Media.DrawingVisual
    $dc = $visual.RenderOpen()
    $dc.PushTransform((New-Object System.Windows.Media.ScaleTransform ($Size / 100.0), ($Size / 100.0)))
    $dc.DrawRoundedRectangle((New-Brush '#12392F'), $null, (New-Object System.Windows.Rect 4, 4, 92, 92), 22, 22)
    $dc.DrawGeometry($null, (New-Pen '#FFFFFF' (8 * $thick)), [System.Windows.Media.Geometry]::Parse('M20,68 A31,31 0 1 1 80,68'))
    $dc.DrawGeometry($null, (New-Pen '#5DCAA5' (8 * $thick)), [System.Windows.Media.Geometry]::Parse('M20,68 A31,31 0 0 1 32,34'))
    $dc.DrawLine((New-Pen '#FAC775' (6 * $thick)), (New-Object System.Windows.Point 50, 64), (New-Object System.Windows.Point 66, 38))
    if (-not $small) {
        $dc.DrawGeometry((New-Brush '#5DCAA5'), $null, [System.Windows.Media.Geometry]::Parse('M50,58 C60,62 60,74 50,80 C40,74 40,62 50,58 Z'))
    }
    $dc.Pop()
    $dc.Close()
    $bitmap = New-Object System.Windows.Media.Imaging.RenderTargetBitmap $Size, $Size, 96, 96, ([System.Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)
    $encoder = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
    $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $stream = New-Object System.IO.MemoryStream
    $encoder.Save($stream)
    return , $stream.ToArray()
}

<# Writes an .ico file that holds the given PNG frames (array of byte arrays) for the given sizes. #>
function Write-Ico([string] $Path, [int[]] $FrameSizes, $Frames) {
    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter $stream
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$FrameSizes.Count)
    $offset = 6 + 16 * $FrameSizes.Count
    for ($i = 0; $i -lt $FrameSizes.Count; $i++) {
        $dim = if ($FrameSizes[$i] -ge 256) { 0 } else { $FrameSizes[$i] }
        $writer.Write([byte]$dim); $writer.Write([byte]$dim); $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32)
        $writer.Write([uint32]$Frames[$i].Length); $writer.Write([uint32]$offset)
        $offset += $Frames[$i].Length
    }
    foreach ($frame in $Frames) { $writer.Write($frame) }
    $writer.Flush()
    [System.IO.File]::WriteAllBytes($Path, $stream.ToArray())
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$frames = @()
foreach ($size in $Sizes) { $frames += , (New-IconPng $size) }
Write-Ico (Join-Path $OutDir 'ecowattsense.ico') $Sizes $frames
[System.IO.File]::WriteAllBytes((Join-Path $OutDir 'ecowattsense-256.png'), $frames[$Sizes.Count - 1])
Write-Host "Wrote $OutDir\ecowattsense.ico ($($Sizes.Count) frames) and ecowattsense-256.png"
