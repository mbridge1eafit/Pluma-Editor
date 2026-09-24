<#
.SYNOPSIS
    Empaqueta la versión Release de Pluma en un archivo .zip y genera el archivo SHA256SUMS.txt.
.DESCRIPTION
    Copia los binarios optimizados, iconos, scripts de asociación y documentación a la carpeta dist/
    y genera un paquete portable listo para publicar en GitHub Releases junto con su suma de verificación SHA-256.
#>

[CmdletBinding()]
param(
    [string]$Version = ""
)

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

$distDir = Join-Path $rootDir "dist"
$stageName = "pluma-$Version-windows-x64"
$stageDir = Join-Path $distDir $stageName
$zipFile = Join-Path $distDir "$stageName.zip"
$checksumFile = Join-Path $distDir "SHA256SUMS.txt"

Write-Host "==> Preparando paquete de distribución para Pluma $Version..." -ForegroundColor Cyan

# Limpiar directorio previo de preparación
if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
if (Test-Path $zipFile) { Remove-Item -Force $zipFile }
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

# Generar Checksum SHA-256
Write-Host "--> Calculando Checksum SHA-256..." -ForegroundColor Gray
$zipHash = (Get-FileHash -Path $zipFile -Algorithm SHA256).Hash.ToLower()
$exeHash = (Get-FileHash -Path $exePath -Algorithm SHA256).Hash.ToLower()

$checksumLines = @(
    "$zipHash  $([System.IO.Path]::GetFileName($zipFile))",
    "$exeHash  pluma.exe"
)
$checksumLines | Out-File -FilePath $checksumFile -Encoding utf8

Write-Host ""
Write-Host "[OK] Paquete generado exitosamente en: $distDir" -ForegroundColor Green
Write-Host "  * Archivo ZIP:  $([System.IO.Path]::GetFileName($zipFile))"
Write-Host "  * Checksums:    SHA256SUMS.txt"
Write-Host ""
Write-Host "Contenido de SHA256SUMS.txt:" -ForegroundColor Yellow
Get-Content $checksumFile
