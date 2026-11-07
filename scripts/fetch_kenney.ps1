# fetch_kenney.ps1 - Batch download Kenney asset packs (CC0) into assets_staging/packs
# Flow per pack: fetch page HTML -> extract data-download-id -> request signed URL -> download zip
# Writes manifest entries to assets_staging/manifest.csv
[CmdletBinding()]
param(
    # Comma-separated slugs (array binding via -Packs 'a,b,c' is unreliable across hosts).
    [string]$PackList = 'pixel-platformer,jumper-pack,impact-sounds,interface-sounds,particle-pack,fantasy-ui-borders,generic-items,ui-pack-rpg-expansion',
    [string]$OutDir = "$PSScriptRoot\..\assets_staging\packs",
    [string]$Manifest = "$PSScriptRoot\..\assets_staging\manifest.csv"
)

$Packs = $PackList -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not (Test-Path $Manifest)) {
    "id,name,source_url,author,license,download_time,size_bytes,sha256,local_path" | Out-File -Encoding utf8 $Manifest
}

function Get-KenneyDownloadUrl([string]$slug) {
    $pageUrl = "https://kenney.nl/assets/$slug"
    $html = (Invoke-WebRequest -Uri $pageUrl -UseBasicParsing -TimeoutSec 60).Content
    $m = [regex]::Match($html, "data-download-id='([^']+)'")
    if (-not $m.Success) { $m = [regex]::Match($html, 'data-download-id="([^"]+)"') }
    if (-not $m.Success) { throw "no data-download-id on $pageUrl" }
    $id = $m.Groups[1].Value
    $resp = Invoke-WebRequest -Uri "https://kenney.nl/download-link/$id" -UseBasicParsing -TimeoutSec 60 `
        -Headers @{ 'Referer' = $pageUrl; 'X-Requested-With' = 'XMLHttpRequest'; 'Accept' = 'application/json' }
    $json = $resp.Content | ConvertFrom-Json
    if (-not $json.url) { throw "no signed url for $slug : $($resp.Content)" }
    return $json.url
}

foreach ($slug in $Packs) {
    try {
        Write-Host "== $slug =="
        $url = Get-KenneyDownloadUrl $slug
        $zipPath = Join-Path $OutDir "$slug.zip"
        Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing -TimeoutSec 300
        $fi = Get-Item $zipPath
        $hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLower()
        $now = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
        # Kenney assets are authored by "Kenney (kenney.nl)" and licensed CC0 unless page says otherwise
        $line = "kenney-$slug,$slug,https://kenney.nl/assets/$slug,Kenney Vleugels (kenney.nl),CC0-1.0,$now,$($fi.Length),$hash,assets_staging/packs/$slug.zip"
        Add-Content -Encoding utf8 -Path $Manifest -Value $line
        Write-Host "   OK $($fi.Length) bytes sha256=$hash"
    } catch {
        Write-Host "   FAIL: $($_.Exception.Message)" -ForegroundColor Red
    }
}
Write-Host "done."
