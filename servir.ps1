# Servidor local mínimo para Chispa EFI (sin instalar nada).
# Uso:  powershell -NoProfile -ExecutionPolicy Bypass -File servir.ps1
# Después abrí http://localhost:8765 en Chrome o Edge (USB y Bluetooth necesitan localhost o https).
# Sirve la carpeta docs/ (el sitio que arma build.sh).
param([int]$Port = $(if ($env:PORT) { [int]$env:PORT } else { 8765 }))
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Join-Path $here 'docs'
if (-not (Test-Path $root)) { $root = $here }
$root = [IO.Path]::GetFullPath($root)
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$Port/")
$listener.Start()
Write-Host "Chispa EFI en http://localhost:$Port  (Ctrl+C para cortar)"
$types = @{ '.html'='text/html; charset=utf-8'; '.js'='text/javascript; charset=utf-8'; '.css'='text/css'; '.json'='application/json';
            '.webmanifest'='application/manifest+json'; '.svg'='image/svg+xml'; '.png'='image/png'; '.csv'='text/csv'; '.ino'='text/plain; charset=utf-8' }
while ($listener.IsListening) {
  $ctx = $listener.GetContext()
  $path = [Uri]::UnescapeDataString($ctx.Request.Url.AbsolutePath.TrimStart('/'))
  if ($path -eq '' -or $path.EndsWith('/')) { $path = $path + 'index.html' }
  $file = [IO.Path]::GetFullPath((Join-Path $root $path))
  if ($file.StartsWith($root) -and (Test-Path $file -PathType Container)) { $file = Join-Path $file 'index.html' }
  if ($file.StartsWith($root) -and (Test-Path $file -PathType Leaf)) {
    $bytes = [IO.File]::ReadAllBytes($file)
    $ext = [IO.Path]::GetExtension($file).ToLower()
    $ctx.Response.ContentType = $(if ($types.ContainsKey($ext)) { $types[$ext] } else { 'application/octet-stream' })
    $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
  } else {
    $ctx.Response.StatusCode = 404
  }
  $ctx.Response.Close()
}
