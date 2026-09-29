<#
.SYNOPSIS
    Genera el paquete MSIX de Pluma para Microsoft Store (dist\pluma-vX.Y.Z-x64.msix).
.DESCRIPTION
    Copia pluma.exe y LICENSE, genera los iconos del paquete a partir de res\icons (varias escalas y
    tamaños), crea resources.pri con makepri para que Windows elija el icono adecuado a cada DPI y
    empaqueta con makeappx, que valida el manifiesto (packaging\msix\AppxManifest.xml).

    La identidad del paquete (nombre, publisher) sale de packaging\msix\identity.json, que se rellena
    con los valores de Partner Center. Mientras tenga valores PENDIENTE, el paquete solo sirve para
    probar en local.

    El paquete NO se firma: Microsoft Store lo firma al publicarlo (Decisión 017). Un MSIX sin firmar no
    se instala con doble clic; para probarlo en local, con el modo de desarrollador de Windows activo:
        Add-AppxPackage -Register dist\msix\AppxManifest.xml
.PARAMETER Version
    Tag de la versión (vX.Y.Z). Por defecto, project(VERSION) de CMakeLists.txt.
.PARAMETER RequireStoreIdentity
    Falla si identity.json conserva los valores PENDIENTE. Úselo al generar el paquete que se sube a
    Partner Center.
#>

[CmdletBinding()]
param(
    [string]$Version = "",
    [switch]$RequireStoreIdentity
)

$ErrorActionPreference = "Stop"
$rootDir = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $rootDir

# Herramientas del Windows SDK: PATH (Developer PowerShell) o la versión más reciente instalada.
function Find-SdkTool([string]$Name) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $kits = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    $found = Get-ChildItem -Path (Join-Path $kits "10.*\x64\$Name") -ErrorAction SilentlyContinue |
        Sort-Object { [version]$_.Directory.Parent.Name } -Descending | Select-Object -First 1
    if (-not $found) { throw "No se encontró $Name. Instale el Windows SDK (Visual Studio Build Tools)." }
    return $found.FullName
}

function Invoke-Tool([string]$Exe, [string[]]$Arguments) {
    $output = & $Exe @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
        throw "Falló $([System.IO.Path]::GetFileName($Exe)) (código $LASTEXITCODE)."
    }
}

# PNG de Size x Size con la imagen de origen centrada, ocupando Fill (0-1) del lado; fondo transparente.
function Save-ScaledPng([System.Drawing.Image]$Source, [string]$Path, [int]$Size, [double]$Fill = 1.0) {
    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $side = [int][math]::Round($Size * $Fill)
        $offset = [int][math]::Floor(($Size - $side) / 2)
        $graphics.DrawImage($Source, (New-Object System.Drawing.Rectangle($offset, $offset, $side, $side)))
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

# Versión: argumento o project(VERSION) de CMakeLists.txt
if ([string]::IsNullOrWhiteSpace($Version)) {
    $cmakeContent = Get-Content "CMakeLists.txt" -Raw
    if ($cmakeContent -match 'project\s*\(\s*\w+\s+VERSION\s+([0-9\.]+)') { $Version = "v$($Matches[1])" }
}
if (-not $Version.StartsWith("v")) { $Version = "v$Version" }
if ($Version -notmatch '^v(\d+)\.(\d+)\.(\d+)$') {
    Write-Error "Versión no válida para MSIX: $Version (se espera vX.Y.Z, sin sufijos)."
    exit 1
}
# Microsoft Store exige que el cuarto número sea 0.
$packageVersion = "$($Matches[1]).$($Matches[2]).$($Matches[3]).0"

$exePath = Join-Path $rootDir "build\release\bin\pluma.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "No se encontró el ejecutable de Release en: $exePath`nCompile primero con: cmake --build --preset release"
    exit 1
}
$exeVersion = (Get-Item $exePath).VersionInfo.ProductVersion
if ($exeVersion -notmatch '^(\d+)\.(\d+)\.(\d+)' -or "v$($Matches[1]).$($Matches[2]).$($Matches[3])" -ne $Version) {
    Write-Error "La versión de pluma.exe ($exeVersion) no coincide con $Version. Actualice project(VERSION) en CMakeLists.txt y recompile."
    exit 1
}

# Identidad de Partner Center
$identity = Get-Content (Join-Path $rootDir "packaging\msix\identity.json") -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($field in "identityName", "publisher", "publisherDisplayName", "displayName") {
    if ([string]::IsNullOrWhiteSpace($identity.$field)) { throw "Falta '$field' en packaging\msix\identity.json." }
}
$placeholder = "$($identity.identityName) $($identity.publisher)" -match 'PENDIENTE'
if ($placeholder -and $RequireStoreIdentity) {
    Write-Error "packaging\msix\identity.json tiene valores PENDIENTE: copie los de Partner Center (ver store\README.md)."
    exit 1
}

$makeappx = Find-SdkTool "makeappx.exe"
$makepri = Find-SdkTool "makepri.exe"
$distDir = Join-Path $rootDir "dist"
$stageDir = Join-Path $distDir "msix"
$assetsDir = Join-Path $stageDir "Assets"
$msixFile = Join-Path $distDir "pluma-$Version-x64.msix"
$priConfig = Join-Path $distDir "priconfig.xml"

Write-Host "==> Preparando el paquete MSIX de Pluma $Version ($packageVersion)..." -ForegroundColor Cyan
if ($placeholder) {
    Write-Warning "identity.json tiene valores PENDIENTE: el paquete sirve para pruebas locales, no para Partner Center."
}
if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
if (Test-Path $msixFile) { Remove-Item -Force $msixFile }
New-Item -ItemType Directory -Force -Path $assetsDir | Out-Null

Copy-Item $exePath -Destination $stageDir
Copy-Item (Join-Path $rootDir "LICENSE") -Destination $stageDir

# Manifiesto: valores escapados para XML, UTF-8 sin BOM
$tokens = [ordered]@{
    "__IDENTITY_NAME__"          = $identity.identityName
    "__PUBLISHER_DISPLAY_NAME__" = $identity.publisherDisplayName
    "__PUBLISHER__"              = $identity.publisher
    "__DISPLAY_NAME__"           = $identity.displayName
    "__VERSION__"                = $packageVersion
}
$manifest = Get-Content (Join-Path $rootDir "packaging\msix\AppxManifest.xml") -Raw -Encoding UTF8
foreach ($token in $tokens.Keys) {
    $manifest = $manifest.Replace($token, [System.Security.SecurityElement]::Escape([string]$tokens[$token]))
}
if ($manifest -match '__[A-Z_]+__') { throw "Quedó un marcador sin reemplazar en el manifiesto: $($Matches[0])" }
[System.IO.File]::WriteAllText((Join-Path $stageDir "AppxManifest.xml"), $manifest, [System.Text.UTF8Encoding]::new($false))

# Iconos. Nombres calificados (scale-*, targetsize-*, altform-*): resources.pri le indica a Windows cuál
# usar según el DPI y el tamaño pedido (Inicio, barra de tareas, Explorador).
Write-Host "--> Generando iconos..." -ForegroundColor Gray
Add-Type -AssemblyName System.Drawing
$appIcon = [System.Drawing.Image]::FromFile((Join-Path $rootDir "res\icons\app.png"))
$docIcon = [System.Drawing.Image]::FromFile((Join-Path $rootDir "res\icons\doc_md.png"))
try {
    $scales = @{ 100 = 1.0; 125 = 1.25; 150 = 1.5; 200 = 2.0; 400 = 4.0 }
    foreach ($scale in $scales.Keys) {
        $factor = $scales[$scale]
        Save-ScaledPng $appIcon (Join-Path $assetsDir "Square44x44Logo.scale-$scale.png") ([int][math]::Round(44 * $factor))
        # Mosaico de Inicio: el icono ocupa dos tercios, como en las plantillas de Visual Studio.
        Save-ScaledPng $appIcon (Join-Path $assetsDir "Square150x150Logo.scale-$scale.png") ([int][math]::Round(150 * $factor)) 0.66
        Save-ScaledPng $appIcon (Join-Path $assetsDir "StoreLogo.scale-$scale.png") ([int][math]::Round(50 * $factor))
    }
    foreach ($size in 16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 256) {
        foreach ($variant in "", "_altform-unplated", "_altform-lightunplated") {
            Save-ScaledPng $appIcon (Join-Path $assetsDir "Square44x44Logo.targetsize-$size$variant.png") $size
            Save-ScaledPng $docIcon (Join-Path $assetsDir "MarkdownFile.targetsize-$size$variant.png") $size
        }
    }
} finally {
    $appIcon.Dispose()
    $docIcon.Dispose()
}

# resources.pri (idioma por defecto: el del manifiesto)
Write-Host "--> Generando resources.pri..." -ForegroundColor Gray
Invoke-Tool $makepri @("createconfig", "/cf", $priConfig, "/dq", "es", "/o")
# Sin <packaging>: la configuración por defecto separa cada escala en un resources.scale-*.pri para
# paquetes de recursos de un bundle; en un MSIX único todas las escalas van en resources.pri.
[xml]$priXml = Get-Content $priConfig -Raw
$packagingNode = $priXml.SelectSingleNode("//packaging")
if ($packagingNode) { [void]$packagingNode.ParentNode.RemoveChild($packagingNode) }
$priXml.Save($priConfig)
Invoke-Tool $makepri @("new", "/pr", $stageDir, "/cf", $priConfig, "/mn", (Join-Path $stageDir "AppxManifest.xml"),
    "/of", (Join-Path $stageDir "resources.pri"), "/o")
Remove-Item -Force $priConfig

# Empaquetar (makeappx valida el manifiesto contra el esquema)
Write-Host "--> Empaquetando con makeappx..." -ForegroundColor Gray
Invoke-Tool $makeappx @("pack", "/d", $stageDir, "/p", $msixFile, "/o")

$sizeKb = [math]::Round((Get-Item $msixFile).Length / 1KB)
Write-Host ""
Write-Host "[OK] Paquete MSIX: $msixFile ($sizeKb KB, versión $packageVersion, sin firmar)" -ForegroundColor Green
Write-Host "  * Identidad: $($identity.identityName) / $($identity.publisher)"
Write-Host "  * Súbalo en Partner Center, sección Paquetes del envío (ver store\README.md o la skill msstore-publish)."
