# Servidor estático mínimo para docs/ (usa só o PowerShell que já vem no Windows).
# Normalmente é chamado por abrir-firmwares.bat.
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$mime = @{
  '.html' = 'text/html; charset=utf-8'; '.js' = 'text/javascript; charset=utf-8'
  '.json' = 'application/json; charset=utf-8'; '.css' = 'text/css; charset=utf-8'
  '.png' = 'image/png'; '.jpg' = 'image/jpeg'; '.svg' = 'image/svg+xml'
}

$listener = $null
foreach ($port in 8765..8775) {
  $l = New-Object System.Net.HttpListener
  $l.Prefixes.Add("http://localhost:$port/")
  try { $l.Start(); $listener = $l; break } catch { $l.Close() }
}
if (-not $listener) { Write-Host 'Nenhuma porta livre entre 8765 e 8775.'; exit 1 }

$url = "http://localhost:$port/firmwares.html"
Write-Host "Servindo $root em http://localhost:$port/"
Write-Host "Abrindo $url"
Write-Host 'Feche esta janela ou aperte Ctrl+C para parar.'
Start-Process $url

try {
  while ($listener.IsListening) {
    $ctx = $listener.GetContext()
    $res = $ctx.Response
    $rel = [Uri]::UnescapeDataString($ctx.Request.Url.AbsolutePath.TrimStart('/'))
    if ($rel -eq '') { $rel = 'firmwares.html' }
    $path = [IO.Path]::GetFullPath((Join-Path $root $rel))
    if ($path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) -and (Test-Path $path -PathType Leaf)) {
      $ext = [IO.Path]::GetExtension($path).ToLower()
      $res.ContentType = if ($mime.ContainsKey($ext)) { $mime[$ext] } else { 'application/octet-stream' }
      $res.Headers.Add('Cache-Control', 'no-store')
      $bytes = [IO.File]::ReadAllBytes($path)
      $res.ContentLength64 = $bytes.Length
      $res.OutputStream.Write($bytes, 0, $bytes.Length)
    } else {
      $res.StatusCode = 404
    }
    $res.Close()
  }
} finally {
  $listener.Stop()
}
