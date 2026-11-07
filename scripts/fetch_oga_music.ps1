# fetch_oga_music.ps1 - Download selected OpenGameArt music tracks (CC0) into assets_staging/music
[CmdletBinding()]
param(
    [string]$OutDir = "$PSScriptRoot\..\assets_staging\music",
    [string]$Manifest = "$PSScriptRoot\..\assets_staging\manifest.csv"
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }
if (-not (Test-Path $Manifest)) {
    "id,name,source_url,author,license,download_time,size_bytes,sha256,local_path" | Out-File -Encoding utf8 $Manifest
}

# slug => preferred file path on opengameart.org (direct file under /sites/default/files/)
$tracks = @(
    @{ slug = 'tragic-ambient-main-menu';  file = 'ambientmain_0.ogg' },
    @{ slug = 'crystal-cave-song18';       file = 'song18.mp3' },
    @{ slug = 'mysterious-ambience-song21';file = 'song21.mp3' },
    @{ slug = 'battle-theme-a';            file = 'battleThemeA.mp3' },
    @{ slug = 'fast-fight-battle-music';   file = 'fight.wav' },
    @{ slug = 'cave-theme';                file = 'cave%20themeb4.ogg' }
)

foreach ($t in $tracks) {
    $slug = $t.slug
    try {
        Write-Host "== $slug =="
        $pageUrl = "https://opengameart.org/content/$slug"
        $html = (Invoke-WebRequest -Uri $pageUrl -UseBasicParsing -TimeoutSec 60).Content
        # author
        $author = 'unknown'
        $am = [regex]::Match($html, '/users/([^"]+)"[^>]*>([^<]+)</a>')
        if ($am.Success) { $author = $am.Groups[2].Value.Trim() }
        # license: look for cc0 / cc-by markers
        $license = 'unknown'
        if ($html -match 'creativecommons\.org/publicdomain/zero') { $license = 'CC0-1.0' }
        elseif ($html -match 'licenses/by-sa/3\.0') { $license = 'CC-BY-SA-3.0' }
        elseif ($html -match 'licenses/by/3\.0') { $license = 'CC-BY-3.0' }
        elseif ($html -match 'licenses/by/4\.0') { $license = 'CC-BY-4.0' }
        elseif ($html -match 'OGA-BY') { $license = 'OGA-BY' }
        $fileUrl = "https://opengameart.org/sites/default/files/$($t.file)"
        $ext = [IO.Path]::GetExtension($t.file)
        $local = Join-Path $OutDir "$slug$ext"
        Invoke-WebRequest -Uri $fileUrl -OutFile $local -UseBasicParsing -TimeoutSec 300
        $fi = Get-Item $local
        $hash = (Get-FileHash $local -Algorithm SHA256).Hash.ToLower()
        $now = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
        Add-Content -Encoding utf8 -Path $Manifest -Value "oga-$slug,$slug,$pageUrl,$author,$license,$now,$($fi.Length),$hash,assets_staging/music/$slug$ext"
        Write-Host "   OK $($fi.Length) bytes author=$author license=$license"
    } catch {
        Write-Host "   FAIL: $($_.Exception.Message)" -ForegroundColor Red
    }
}
Write-Host "done."
