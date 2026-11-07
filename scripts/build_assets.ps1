# build_assets.ps1 - build the deployable assets/ folder from assets_staging/ + data/.
# - copies config tables / room templates / strings
# - copies + tints the Kenney pixel-platformer atlases (gfx/)
# - converts selected Kenney SFX (ogg -> wav) and music (ogg/mp3 -> mp3) via ffmpeg
# - writes assets/LICENSES.txt
# All CC0 sources; see assets_staging/manifest.csv for hashes/URLs.
$ErrorActionPreference = 'Stop'
$Root   = Split-Path -Parent $PSScriptRoot
$Stg    = Join-Path $Root 'assets_staging'
$Data   = Join-Path $Root 'data'
$Out    = Join-Path $Root 'assets'
$Ffmpeg = 'C:\Users\Administrator\Downloads\ffmpeg-7.0.2-essentials_build\bin\ffmpeg.exe'

if (-not (Test-Path $Ffmpeg)) { throw "ffmpeg not found at $Ffmpeg" }

# ---- clean output ----
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
foreach ($d in 'gfx','sfx','bgm','tables','rooms') {
    New-Item -ItemType Directory -Force -Path (Join-Path $Out $d) | Out-Null
}

# ---- data tables / rooms / strings ----
Copy-Item (Join-Path $Data 'tables\*.csv')   (Join-Path $Out 'tables\')
Copy-Item (Join-Path $Data 'rooms\*.txt')    (Join-Path $Out 'rooms\')
Copy-Item (Join-Path $Data 'strings_zh.csv') $Out

# ---- gfx: character atlas + tinted tile atlases ----
$pp = Join-Path $Stg 'extracted\pixel-platformer\Tilemap'
Copy-Item (Join-Path $pp 'tilemap-characters_packed.png') (Join-Path $Out 'gfx\chars.png')
python (Join-Path $PSScriptRoot 'tint_tiles.py') (Join-Path $pp 'tilemap_packed.png') (Join-Path $Out 'gfx')

# ---- sfx: ogg -> wav (pcm_s16le 44.1k) ----
$ken = Join-Path $Stg 'extracted'
$sfxMap = [ordered]@{
    swing   = 'interface-sounds\Audio\scratch_001.ogg'
    hit     = 'impact-sounds\Audio\impactPunch_medium_000.ogg'
    kill    = 'impact-sounds\Audio\impactPunch_heavy_000.ogg'
    hurt    = 'impact-sounds\Audio\impactSoft_heavy_000.ogg'
    die     = 'impact-sounds\Audio\impactGlass_heavy_000.ogg'
    jump    = 'impact-sounds\Audio\footstep_grass_000.ogg'
    roll    = 'interface-sounds\Audio\switch_001.ogg'
    coin    = 'interface-sounds\Audio\pluck_001.ogg'
    soul    = 'interface-sounds\Audio\confirmation_004.ogg'
    potion  = 'interface-sounds\Audio\glass_001.ogg'
    buy     = 'interface-sounds\Audio\confirmation_001.ogg'
    error   = 'interface-sounds\Audio\error_001.ogg'
    ui      = 'interface-sounds\Audio\tick_001.ogg'
    uiyes   = 'interface-sounds\Audio\select_001.ogg'
    portal  = 'interface-sounds\Audio\bong_001.ogg'
    bossdie = 'impact-sounds\Audio\impactBell_heavy_000.ogg'
    slam    = 'impact-sounds\Audio\impactPlate_heavy_000.ogg'
    boom    = 'impact-sounds\Audio\impactWood_heavy_000.ogg'
    charge  = 'interface-sounds\Audio\maximize_001.ogg'
    door    = 'impact-sounds\Audio\impactMetal_heavy_000.ogg'
}
foreach ($k in $sfxMap.Keys) {
    $srcSfx = Join-Path $ken $sfxMap[$k]
    if (-not (Test-Path $srcSfx)) { throw "missing sfx source: $srcSfx" }
    & $Ffmpeg -y -loglevel error -i $srcSfx -acodec pcm_s16le -ar 44100 (Join-Path $Out "sfx\$k.wav")
}

# ---- bgm -> mp3 ----
$mus = Join-Path $Stg 'music'
& $Ffmpeg -y -loglevel error -i (Join-Path $mus 'tragic-ambient-main-menu.ogg') -codec:a libmp3lame -q:a 4 (Join-Path $Out 'bgm\title.mp3')
& $Ffmpeg -y -loglevel error -i (Join-Path $mus 'cave-theme.ogg')               -codec:a libmp3lame -q:a 4 (Join-Path $Out 'bgm\cave.mp3')
Copy-Item (Join-Path $mus 'mysterious-ambience-song21.mp3') (Join-Path $Out 'bgm\amb.mp3')
Copy-Item (Join-Path $mus 'battle-theme-a.mp3')             (Join-Path $Out 'bgm\boss.mp3')

# ---- licenses ----
$lic = @'
Selected assets licenses (all CC0 1.0, public domain dedication):
- Kenney "Pixel Platformer" (tile/character atlases) - Kenney Vleugels - https://kenney.nl/assets/pixel-platformer - CC0
- Kenney "Impact Sounds" (sfx) - Kenney Vleugels - https://kenney.nl/assets/impact-sounds - CC0
- Kenney "Interface Sounds" (sfx) - Kenney Vleugels - https://kenney.nl/assets/interface-sounds - CC0
- "Tragic Ambient Main Menu" (title music) - HaelDB - https://opengameart.org/content/tragic-ambient-main-menu - CC0
- "Cave Theme" (level 1 music) - HaelDB - https://opengameart.org/content/cave-theme - CC0
- "Mysterious Ambience Song21" (level 2/3 music) - cynicmusic - https://opengameart.org/content/mysterious-ambience-song21 - CC0
- "Battle Theme A" (boss music) - cynicmusic - https://opengameart.org/content/battle-theme-a - CC0
Full source manifest with hashes: assets_staging/manifest.csv (not shipped).
'@
Set-Content -Path (Join-Path $Out 'LICENSES.txt') -Value $lic -Encoding UTF8

Write-Host "assets built -> $Out"
