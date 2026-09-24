<#
.SYNOPSIS
    Genera notas de version (Release Notes) en formato Markdown a partir del historial de commits de Git.
.DESCRIPTION
    Analiza los commits convencionales (feat, fix, perf, docs, chore, etc.) entre el ultimo tag
    y HEAD (o desde el inicio si no hay tags previos), y genera un archivo Markdown formateado
    listo para usar en GitHub Releases.
#>

[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$OutputPath = "",
    [string]$FromTag = "",
    [string]$ToCommit = "HEAD"
)

$ErrorActionPreference = "Stop"
$rootDir = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $rootDir

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $rootDir "dist\RELEASE_NOTES.md"
}
$OutputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)

# Detectar version si no se especifico
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

# Determinar el rango de commits: desde el tag anterior a esta version.
# En CI el tag de la version ya existe y apunta a HEAD, asi que se excluye (si no, el rango quedaria vacio).
if ([string]::IsNullOrWhiteSpace($FromTag)) {
    $toSha = (git rev-parse "$ToCommit^{commit}").Trim()
    foreach ($tag in @(git tag -l "v*" --sort=-v:refname)) {
        $tag = $tag.Trim()
        if ([string]::IsNullOrWhiteSpace($tag) -or $tag -eq $Version) { continue }
        $tagSha = (git rev-parse "$tag^{commit}").Trim()
        if ($tagSha -eq $toSha) { continue }
        git merge-base --is-ancestor $tagSha $toSha 2>$null
        if ($LASTEXITCODE -eq 0) {
            $FromTag = $tag
            break
        }
    }
}

if (-not [string]::IsNullOrWhiteSpace($FromTag)) {
    Write-Host "Analizando commits desde $FromTag hasta $ToCommit..." -ForegroundColor Cyan
    $commits = git log "$FromTag..$ToCommit" --no-merges --pretty=format:"%h%x09%s"
} else {
    Write-Host "No se encontraron tags previos. Analizando todos los commits hasta $ToCommit..." -ForegroundColor Cyan
    $commits = git log "$ToCommit" --no-merges --pretty=format:"%h%x09%s"
}

$features = [System.Collections.Generic.List[string]]::new()
$fixes = [System.Collections.Generic.List[string]]::new()
$performance = [System.Collections.Generic.List[string]]::new()
$docs = [System.Collections.Generic.List[string]]::new()
$others = [System.Collections.Generic.List[string]]::new()

foreach ($line in $commits) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line -split "`t", 2
    if ($parts.Count -lt 2) { continue }
    $hash = $parts[0]
    $subject = $parts[1]

    # Clasificacion por Conventional Commits
    if ($subject -match '^(feat)(\(.*\))?:\s*(.+)$') {
        $scope = $Matches[2]
        $msg = $Matches[3]
        $entry = if ($scope) { "**$($scope.Trim('()'))**: $msg ($hash)" } else { "$msg ($hash)" }
        $features.Add($entry)
    } elseif ($subject -match '^(fix)(\(.*\))?:\s*(.+)$') {
        $scope = $Matches[2]
        $msg = $Matches[3]
        $entry = if ($scope) { "**$($scope.Trim('()'))**: $msg ($hash)" } else { "$msg ($hash)" }
        $fixes.Add($entry)
    } elseif ($subject -match '^(perf)(\(.*\))?:\s*(.+)$') {
        $scope = $Matches[2]
        $msg = $Matches[3]
        $entry = if ($scope) { "**$($scope.Trim('()'))**: $msg ($hash)" } else { "$msg ($hash)" }
        $performance.Add($entry)
    } elseif ($subject -match '^(docs)(\(.*\))?:\s*(.+)$') {
        $scope = $Matches[2]
        $msg = $Matches[3]
        $entry = if ($scope) { "**$($scope.Trim('()'))**: $msg ($hash)" } else { "$msg ($hash)" }
        $docs.Add($entry)
    } else {
        $others.Add("$subject ($hash)")
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Pluma $Version")
$lines.Add('')
$lines.Add('Editor Markdown 100% nativo para Windows (Win32 / Direct2D / C++20).')
$lines.Add('')

if ($features.Count -gt 0) {
    $lines.Add('### Nuevas Caracteristicas (Features)')
    foreach ($item in $features) {
        $lines.Add("- $item")
    }
    $lines.Add('')
}

if ($fixes.Count -gt 0) {
    $lines.Add('### Correcciones de Errores (Bug Fixes)')
    foreach ($item in $fixes) {
        $lines.Add("- $item")
    }
    $lines.Add('')
}

if ($performance.Count -gt 0) {
    $lines.Add('### Rendimiento y Optimizaciones (Performance)')
    foreach ($item in $performance) {
        $lines.Add("- $item")
    }
    $lines.Add('')
}

if ($docs.Count -gt 0) {
    $lines.Add('### Documentacion (Documentation)')
    foreach ($item in $docs) {
        $lines.Add("- $item")
    }
    $lines.Add('')
}

if ($others.Count -gt 0) {
    $lines.Add('### Mantenimiento y Otros Cambios (Chores & Maintenance)')
    foreach ($item in $others) {
        $lines.Add("- $item")
    }
    $lines.Add('')
}

# Integrar Checksums si existen
$checksumPath = Join-Path $rootDir "dist\SHA256SUMS.txt"
if (Test-Path $checksumPath) {
    $lines.Add('### Verificacion de Integridad (SHA-256 Checksums)')
    $lines.Add('```text')
    $checksumContent = Get-Content $checksumPath -Raw
    $lines.Add($checksumContent.Trim())
    $lines.Add('```')
    $lines.Add('')
}

# Instrucciones de ejecucion en Windows
$lines.Add('### Instrucciones para Windows')
$lines.Add('1. Descarga y descomprime el archivo ZIP en la carpeta de tu preferencia.')
$lines.Add('2. Desbloquear en Windows: Haz clic derecho sobre el ZIP o pluma.exe -> Propiedades -> marca Desbloquear (Unblock) -> Aceptar.')
$lines.Add('3. Ejecuta pluma.exe directamente. Es totalmente portable y no requiere instalacion ni dependencias adicionales.')

# Guardar archivo de salida
$outDir = Split-Path -Parent $OutputPath
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}
$content = $lines -join "`r`n"
# UTF-8 sin BOM: el archivo se publica tal cual como cuerpo del Release en GitHub.
[System.IO.File]::WriteAllText($OutputPath, $content, [System.Text.UTF8Encoding]::new($false))

Write-Host "[OK] Release notes generadas en: $OutputPath" -ForegroundColor Green
Write-Host ""
Write-Host "Vista previa de las notas:" -ForegroundColor Yellow
Write-Host "--------------------------------------------------"
Write-Host $content
Write-Host "--------------------------------------------------"
