# Genera los íconos PNG de Chispa (rayo oscuro sobre fondo ámbar).
# Uso: powershell -NoProfile -ExecutionPolicy Bypass -File iconos.ps1
Add-Type -AssemblyName System.Drawing
$dir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'icons'
New-Item -ItemType Directory -Force $dir | Out-Null

# Rayo en una grilla de 24 × 24 (el mismo del logo de la app)
$bolt = @(@(13.5, 2), @(5, 13.4), @(10.6, 13.4), @(9.6, 22), @(18.9, 9.8), @(13.2, 9.8))

function Dibujar([int]$size, [double]$zona, [string]$archivo) {
  $bmp = New-Object System.Drawing.Bitmap $size, $size
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.Clear([System.Drawing.Color]::FromArgb(255, 242, 169, 59))      # ámbar #F2A93B
  $lado = $size * $zona; $off = ($size - $lado) / 2; $esc = $lado / 24
  $pts = foreach ($p in $bolt) { New-Object System.Drawing.PointF ([single]($off + $p[0] * $esc)), ([single]($off + $p[1] * $esc)) }
  $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 28, 19, 5))  # #1C1305
  $g.FillPolygon($brush, [System.Drawing.PointF[]]$pts)
  $bmp.Save((Join-Path $dir $archivo), [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
}
# "zona" deja margen para el recorte circular de Android (íconos maskable)
Dibujar 192 0.62 'icon-192.png'
Dibujar 512 0.62 'icon-512.png'
Write-Host "Íconos listos en $dir"
