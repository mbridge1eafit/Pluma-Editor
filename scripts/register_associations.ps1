<#
.SYNOPSIS
    Registra la asociación de archivos .md, .markdown y .mdown para Pluma con su icono de documento.
.DESCRIPTION
    Configura en el registro de Windows (HKCU:\Software\Classes) la asociación de archivos Markdown
    con Pluma, asignando el icono IDI_DOC_MD (índice 1 del ejecutable o doc_md.ico) y notificando al shell
    de Windows para refrescar los iconos de inmediato sin requerir permisos de administrador.
#>

[CmdletBinding()]
param(
    [string]$PlumaExePath = ""
)

$ErrorActionPreference = "Stop"

# Localizar pluma.exe
if ([string]::IsNullOrWhiteSpace($PlumaExePath)) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $rootDir = Split-Path -Parent $scriptDir
    $candidates = @(
        (Join-Path $rootDir "build\release\bin\pluma.exe"),
        (Join-Path $rootDir "build\debug\bin\pluma.exe"),
        (Join-Path $rootDir "bin\pluma.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) {
            $PlumaExePath = (Resolve-Path $c).Path
            break
        }
    }
}

if (-not (Test-Path $PlumaExePath)) {
    Write-Error "No se encontró pluma.exe. Especifique la ruta con -PlumaExePath o compile el proyecto primero."
    return
}

$rootDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$iconPath = Join-Path $rootDir "markdown.ico"
if (-not (Test-Path $iconPath)) {
    $iconPath = Join-Path $rootDir "res\icons\doc_md.ico"
}
$iconRef = if (Test-Path $iconPath) { (Resolve-Path $iconPath).Path } else { "$PlumaExePath,1" }

Write-Host "Registrando asociaciones para Pluma..." -ForegroundColor Cyan
Write-Host "  Ejecutable: $PlumaExePath"
Write-Host "  Icono:      $iconRef"

$progId = "Pluma.Markdown"
$openCommand = "`"$PlumaExePath`" `"%1`""

# 1. Crear ProgID Pluma.Markdown
$progIdKey = "HKCU:\Software\Classes\$progId"
New-Item -Path $progIdKey -Force | Out-Null
Set-ItemProperty -Path $progIdKey -Name "(default)" -Value "Documento Markdown"
Set-ItemProperty -Path $progIdKey -Name "FriendlyTypeName" -Value "Documento Markdown"

# DefaultIcon
$defaultIconKey = Join-Path $progIdKey "DefaultIcon"
New-Item -Path $defaultIconKey -Force | Out-Null
Set-ItemProperty -Path $defaultIconKey -Name "(default)" -Value "$iconRef"

# Shell open command
$shellOpenCmdKey = Join-Path $progIdKey "shell\open\command"
New-Item -Path $shellOpenCmdKey -Force | Out-Null
Set-ItemProperty -Path $shellOpenCmdKey -Name "(default)" -Value $openCommand

# 2. Asociar extensiones
$extensions = @(".md", ".markdown", ".mdown")
foreach ($ext in $extensions) {
    $extKey = "HKCU:\Software\Classes\$ext"
    New-Item -Path $extKey -Force | Out-Null
    Set-ItemProperty -Path $extKey -Name "(default)" -Value $progId
    Set-ItemProperty -Path $extKey -Name "Content Type" -Value "text/markdown"
    Set-ItemProperty -Path $extKey -Name "PerceivedType" -Value "document"

    $openWithProgidsKey = Join-Path $extKey "OpenWithProgids"
    New-Item -Path $openWithProgidsKey -Force | Out-Null
    Set-ItemProperty -Path $openWithProgidsKey -Name $progId -Value ([byte[]]@())
}

# 3. Notificar a Windows Shell para refrescar iconos (SHChangeNotify)
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class ShellNotify {
    [DllImport("shell32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern void SHChangeNotify(int wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@ -ErrorAction SilentlyContinue

try {
    # SHCNE_ASSOCCHANGED = 0x08000000, SHCNF_IDLIST = 0
    [ShellNotify]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)
    Write-Host "Caché de iconos de Windows actualizada correctamente." -ForegroundColor Green
} catch {
    Write-Warning "No se pudo invocar SHChangeNotify, los iconos se actualizarán al reiniciar la sesión."
}

Write-Host "Asociación completada con éxito. Los archivos .md ahora tienen el icono de Pluma Markdown." -ForegroundColor Green
