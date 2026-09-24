<#
.SYNOPSIS
    Elimina la asociación de archivos Markdown para Pluma del registro.
#>

[CmdletBinding()]
param()

$progId = "Pluma.Markdown"
$progIdKey = "HKCU:\Software\Classes\$progId"

if (Test-Path $progIdKey) {
    Remove-Item -Path $progIdKey -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "ProgID $progId eliminado." -ForegroundColor Yellow
}

$extensions = @(".md", ".markdown", ".mdown")
foreach ($ext in $extensions) {
    $extKey = "HKCU:\Software\Classes\$ext"
    if (Test-Path $extKey) {
        $val = (Get-ItemProperty -Path $extKey -Name "(default)" -ErrorAction SilentlyContinue)."(default)"
        if ($val -eq $progId) {
            Remove-ItemProperty -Path $extKey -Name "(default)" -ErrorAction SilentlyContinue
        }
        $owKey = Join-Path $extKey "OpenWithProgids"
        if (Test-Path $owKey) {
            Remove-ItemProperty -Path $owKey -Name $progId -ErrorAction SilentlyContinue
        }
    }
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class ShellNotifyUnreg {
    [DllImport("shell32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern void SHChangeNotify(int wEventId, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
"@ -ErrorAction SilentlyContinue

try {
    [ShellNotifyUnreg]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)
} catch {}

Write-Host "Asociaciones eliminadas y caché de iconos actualizada." -ForegroundColor Green
