<#
.SYNOPSIS
    Automatiza el flujo completo de lanzamiento de una nueva version para Pluma.
.DESCRIPTION
    1. Verifica el estado del repositorio.
    2. Compila en Release y ejecuta los tests unitarios.
    3. Empaqueta el archivo ZIP portable y calcula el Checksum SHA-256.
    4. Genera las Release Notes categorizadas en Markdown.
    5. Publica mediante Git Tag (GitHub Actions) o directamente con GitHub CLI (gh).
#>

[CmdletBinding()]
param(
    [string]$Version = "",
    [ValidateSet("github-actions", "gh-cli", "dry-run")]
    [string]$Mode = "github-actions",
    [switch]$SkipTests,
    [switch]$SkipBuild,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$rootDir = (Resolve-Path "$PSScriptRoot\..").Path
Set-Location $rootDir

# Detectar version
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

# El cuerpo del Release en GitHub debe ser siempre dist/RELEASE_NOTES.md (nunca las notas automaticas de GitHub).
function Test-ReleaseNotesPublished([string]$Tag) {
    $body = (gh release view $Tag --json body --jq .body) | Out-String
    if ($LASTEXITCODE -ne 0 -or -not $body.Contains("# Pluma $Tag")) {
        Write-Error "El Release $Tag no tiene publicadas las notas generadas. Corrigelo con: gh release edit $Tag --notes-file dist/RELEASE_NOTES.md"
        exit 1
    }
    Write-Host "[OK] Las notas de version estan publicadas en el Release $Tag." -ForegroundColor Green
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "         Lanzamiento de Pluma $Version ($Mode)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Verificar cambios sin confirmar en Git
$gitStatus = git status --porcelain
if (-not [string]::IsNullOrWhiteSpace($gitStatus)) {
    Write-Warning "Tienes cambios sin confirmar en el repositorio:"
    git status --short
    if (-not $Force -and $Mode -ne "dry-run") {
        $confirm = Read-Host "¿Deseas continuar de todas formas? (s/n)"
        if ($confirm -ne 's' -and $confirm -ne 'S') {
            Write-Error "Operacion cancelada por el usuario."
            exit 1
        }
    }
}

# 2. Compilar Release si es necesario
if (-not $SkipBuild) {
    Write-Host "`n[1/4] Compilando Release..." -ForegroundColor Green
    & cmake --build --preset release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Fallo la compilacion en Release."
        exit 1
    }
}

# 3. Ejecutar pruebas unitarias
if (-not $SkipTests) {
    Write-Host "`n[2/4] Ejecutando pruebas unitarias..." -ForegroundColor Green
    & ctest --preset release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Una o mas pruebas fallaron. Abortando lanzamiento."
        exit 1
    }
}

# 4. Empaquetar y generar Checksums
Write-Host "`n[3/4] Empaquetando ZIP y generando SHA-256..." -ForegroundColor Green
& "$PSScriptRoot\package_release.ps1" -Version $Version

# 5. Generar Release Notes
Write-Host "`n[4/4] Generando Release Notes..." -ForegroundColor Green
& "$PSScriptRoot\generate_release_notes.ps1" -Version $Version

Write-Host "`n==========================================================" -ForegroundColor Cyan
Write-Host "       Paquete y Notas de Version listos en dist/" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 6. Publicacion
if ($Mode -eq "dry-run") {
    Write-Host "`nModo 'dry-run' finalizado. No se subio nada a GitHub." -ForegroundColor Yellow
    Write-Host "Para publicar via GitHub Actions ejecuta:"
    Write-Host "  git tag -a $Version -m ""Release $Version""" -ForegroundColor Gray
    Write-Host "  git push origin $Version" -ForegroundColor Gray
    Write-Host "Para publicar via GitHub CLI ejecuta:"
    Write-Host "  gh release create $Version dist/pluma-$Version-windows-x64.zip dist/pluma-$Version-setup-x64.exe dist/SHA256SUMS.txt -F dist/RELEASE_NOTES.md -t ""Pluma $Version""" -ForegroundColor Gray
    return
}

if ($Mode -eq "github-actions") {
    Write-Host "`nPublicando mediante Git Tag y GitHub Actions..." -ForegroundColor Green
    
    # Comprobar si el tag ya existe
    $existingTag = git tag -l $Version
    if (-not [string]::IsNullOrWhiteSpace($existingTag)) {
        Write-Warning "El tag $Version ya existe localmente."
        $overwrite = Read-Host "¿Deseas sobrescribir el tag existente? (s/n)"
        if ($overwrite -eq 's' -or $overwrite -eq 'S') {
            git tag -d $Version
        } else {
            exit 1
        }
    }

    Write-Host "Creando tag local: $Version" -ForegroundColor Gray
    git tag -a $Version -m "Release $Version"

    Write-Host "Enviando tag a origin ($Version)..." -ForegroundColor Cyan
    git push origin $Version

    Write-Host "`n[EXITO] Tag $Version enviado. El workflow de GitHub Actions se ha iniciado para publicar el Release." -ForegroundColor Green
    Write-Host "Puedes seguir el progreso en: https://github.com/mbridge1eafit/Pluma-Editor/actions" -ForegroundColor Cyan

    # El workflow genera y publica las notas con los checksums de sus propios artefactos; se verifica al terminar.
    if (Get-Command gh -ErrorAction SilentlyContinue) {
        Write-Host "`nEsperando al workflow de Release para verificar las notas publicadas..." -ForegroundColor Gray
        $runId = $null
        for ($i = 0; $i -lt 12 -and -not $runId; $i++) {
            Start-Sleep -Seconds 5
            $runId = gh run list --workflow release.yml --branch $Version --limit 1 --json databaseId --jq '.[0].databaseId'
        }
        if (-not $runId) {
            Write-Error "No se encontro la ejecucion del workflow de Release para $Version."
            exit 1
        }
        gh run watch $runId --exit-status
        if ($LASTEXITCODE -ne 0) {
            Write-Error "El workflow de Release fallo. Revisa: https://github.com/mbridge1eafit/Pluma-Editor/actions/runs/$runId"
            exit 1
        }
        Test-ReleaseNotesPublished $Version
    } else {
        Write-Warning "GitHub CLI (gh) no esta disponible: verifica manualmente que el Release $Version muestra las notas generadas."
    }
    return
}

if ($Mode -eq "gh-cli") {
    Write-Host "`nPublicando directamente con GitHub CLI (gh)..." -ForegroundColor Green
    $zipPath = "dist/pluma-$Version-windows-x64.zip"
    $setupPath = "dist/pluma-$Version-setup-x64.exe"
    $checksumPath = "dist/SHA256SUMS.txt"
    $notesPath = "dist/RELEASE_NOTES.md"

    # El actualizador integrado en Pluma necesita el instalador publicado.
    if (-not (Test-Path $setupPath)) {
        Write-Error "No se encontró $setupPath. Instale Inno Setup (winget install --id JRSoftware.InnoSetup -e) y vuelva a empaquetar."
        exit 1
    }

    & gh release create $Version "$zipPath" "$setupPath" "$checksumPath" --title "Pluma $Version" --notes-file "$notesPath"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n[EXITO] Release $Version publicado exitosamente en GitHub!" -ForegroundColor Green
        Test-ReleaseNotesPublished $Version
    } else {
        Write-Error "Fallo la publicacion con gh release create."
    }
}
