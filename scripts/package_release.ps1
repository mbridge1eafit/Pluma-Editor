<#
.SYNOPSIS
    Empaqueta la versión Release de Pluma (ZIP portable + instalador) y genera el archivo SHA256SUMS.txt.
.DESCRIPTION
    Copia los binarios optimizados, iconos, scripts de asociación y documentación a la carpeta dist/,
    genera el paquete portable y compila el instalador (installer/pluma.iss) con Inno Setup, listos para
    publicar en GitHub Releases junto con sus sumas de verificación SHA-256.
    El actualizador integrado en Pluma descarga el instalador y lo verifica con SHA256SUMS.txt.
.PARAMETER RequireInstaller
    Falla si Inno Setup (ISCC.exe) no está disponible, en lugar de publicar solo el ZIP. Lo usa CI.
#>

[CmdletBinding()]
param(
    [string]$Version = "",
    [switch]$RequireInstaller
)

# Inno Setup: PATH, instalación para todos los usuarios o por usuario (winget --scope user).
function Find-InnoSetupCompiler {
    $cmd = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $roots = @(${env:ProgramFiles(x86)}, $env:ProgramFiles, (Join-Path $env:LOCALAPPDATA "Programs")) | Where-Object { $_ }
    foreach ($root in $roots) {
        $found = Get-ChildItem -Path (Join-Path $root "Inno Setup*\ISCC.exe") -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($found) { return $found.FullName }
    }
    return $null
}

$ErrorActionPreference = "Stop"
$rootDir = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $rootDir

# Detectar versión si no se proporciona
if ([string]::IsNullOrWhiteSpace($Version)) {
    if (Test-Path "CMakeLists.txt") {
        $cmakeContent = Get-Content "CMakeLists.txt" -Raw
        if ($cmakeContent -match 'project\s*\(\s*\w+\s+VERSION\s+([0-9\.]+)') {
            $Version = "v$($Matches[1])"
        }
    }
    if ([string]::IsNullOrWhiteSpace($Version)) {
        $Version = "v0.1.0"
    }
}

if (-not $Version.StartsWith("v")) {
    $Version = "v$Version"
}

$exePath = Join-Path $rootDir "build\release\bin\pluma.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "No se encontró el ejecutable de Release en: $exePath`nPor favor compile primero con: cmake --build --preset release"
    exit 1
}

# El actualizador compara la versión de pluma.exe con el tag publicado: si no coinciden, ofrecería
# la misma actualización una y otra vez.
$exeVersion = (Get-Item $exePath).VersionInfo.ProductVersion
if ($exeVersion -notmatch '^(\d+)\.(\d+)\.(\d+)' -or "v$($Matches[1]).$($Matches[2]).$($Matches[3])" -ne ($Version -replace '-.*$', '')) {
    Write-Error "La versión de pluma.exe ($exeVersion) no coincide con $Version. Actualice project(VERSION) en CMakeLists.txt y recompile."
    exit 1
}

$distDir = Join-Path $rootDir "dist"
$stageName = "pluma-$Version-windows-x64"
$stageDir = Join-Path $distDir $stageName
$zipFile = Join-Path $distDir "$stageName.zip"
$setupName = "pluma-$Version-setup-x64.exe"
$setupFile = Join-Path $distDir $setupName
$checksumFile = Join-Path $distDir "SHA256SUMS.txt"

Write-Host "==> Preparando paquete de distribución para Pluma $Version..." -ForegroundColor Cyan

# Limpiar directorio previo de preparación
if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
if (Test-Path $zipFile) { Remove-Item -Force $zipFile }
if (Test-Path $setupFile) { Remove-Item -Force $setupFile }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

# Copiar archivos esenciales
Write-Host "--> Copiando archivos..." -ForegroundColor Gray
Copy-Item $exePath -Destination $stageDir
if (Test-Path "README.md") { Copy-Item "README.md" -Destination $stageDir }
if (Test-Path "markdown.ico") { Copy-Item "markdown.ico" -Destination $stageDir }

# Copiar scripts de asociación
$stageScripts = Join-Path $stageDir "scripts"
New-Item -ItemType Directory -Force -Path $stageScripts | Out-Null
if (Test-Path "scripts\register_associations.ps1") { Copy-Item "scripts\register_associations.ps1" -Destination $stageScripts }
if (Test-Path "scripts\unregister_associations.ps1") { Copy-Item "scripts\unregister_associations.ps1" -Destination $stageScripts }

# Crear archivo ZIP
Write-Host "--> Comprimiendo en $zipFile..." -ForegroundColor Gray
Compress-Archive -Path "$stageDir\*" -DestinationPath $zipFile -Force

# Compilar el instalador (Inno Setup)
$iscc = Find-InnoSetupCompiler
if ($iscc) {
    Write-Host "--> Compilando el instalador con $iscc..." -ForegroundColor Gray
    $isccVersion = $Version.TrimStart("v")
    & $iscc "/Q" "/DAppVersion=$isccVersion" "/DSourceExe=$exePath" "/DOutputDir=$distDir" (Join-Path $rootDir "installer\pluma.iss")
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $setupFile)) {
        Write-Error "Falló la compilación del instalador (installer\pluma.iss)."
        exit 1
    }
} elseif ($RequireInstaller) {
    Write-Error "No se encontró Inno Setup 6 (ISCC.exe). Instálelo con: winget install --id JRSoftware.InnoSetup -e"
    exit 1
} else {
    Write-Warning "No se encontró Inno Setup 6 (ISCC.exe): se omite el instalador. Instálelo con: winget install --id JRSoftware.InnoSetup -e"
}

# Generar Checksum SHA-256
Write-Host "--> Calculando Checksum SHA-256..." -ForegroundColor Gray
$zipHash = (Get-FileHash -Path $zipFile -Algorithm SHA256).Hash.ToLower()
$exeHash = (Get-FileHash -Path $exePath -Algorithm SHA256).Hash.ToLower()

$checksumLines = @(
    "$zipHash  $([System.IO.Path]::GetFileName($zipFile))"
)
if (Test-Path $setupFile) {
    $setupHash = (Get-FileHash -Path $setupFile -Algorithm SHA256).Hash.ToLower()
    $checksumLines += "$setupHash  $setupName"
}
$checksumLines += "$exeHash  pluma.exe"
# UTF-8 sin BOM (PowerShell 5.1 añade BOM con -Encoding utf8)
[System.IO.File]::WriteAllText($checksumFile, (($checksumLines -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))

Write-Host ""
Write-Host "[OK] Paquete generado exitosamente en: $distDir" -ForegroundColor Green
Write-Host "  * Archivo ZIP:  $([System.IO.Path]::GetFileName($zipFile))"
if (Test-Path $setupFile) { Write-Host "  * Instalador:   $setupName" }
Write-Host "  * Checksums:    SHA256SUMS.txt"
Write-Host ""
Write-Host "Contenido de SHA256SUMS.txt:" -ForegroundColor Yellow
Get-Content $checksumFile
