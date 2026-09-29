<#
.SYNOPSIS
    Gets the Microsoft Store MSIX of a Pluma version and checks it before it is uploaded to Partner Center.
.DESCRIPTION
    Default: downloads the "pluma-vX.Y.Z-msix" artifact of the Release workflow run for the tag vX.Y.Z
    (built in CI from the tagged commit, after the tests passed) into dist\store\.
    -Local: builds it here instead with scripts\package_msix.ps1 -RequireStoreIdentity (needs
    build\release\bin\pluma.exe of that version).

    Then it opens the package (an MSIX is a ZIP) and checks that AppxManifest.xml carries the Partner
    Center identity of packaging\msix\identity.json and the version X.Y.Z.0. Prints the path and SHA-256.
.PARAMETER Version
    vX.Y.Z. Default: project(VERSION) of CMakeLists.txt.
.PARAMETER Local
    Build the package locally instead of downloading the CI artifact.
#>

[CmdletBinding()]
param(
    [string]$Version = "",
    [switch]$Local
)

$ErrorActionPreference = "Stop"
$rootDir = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path
Set-Location $rootDir

if ([string]::IsNullOrWhiteSpace($Version)) {
    if ((Get-Content "CMakeLists.txt" -Raw) -match 'project\s*\(\s*\w+\s+VERSION\s+([0-9\.]+)') { $Version = "v$($Matches[1])" }
}
if (-not $Version.StartsWith("v")) { $Version = "v$Version" }
if ($Version -notmatch '^v(\d+)\.(\d+)\.(\d+)$') { throw "Invalid version: $Version (expected vX.Y.Z)." }
$packageVersion = "$($Matches[1]).$($Matches[2]).$($Matches[3]).0"
$msixName = "pluma-$Version-x64.msix"

if ($Local) {
    & (Join-Path $rootDir "scripts\package_msix.ps1") -Version $Version -RequireStoreIdentity
    $msixPath = Join-Path $rootDir "dist\$msixName"
} else {
    $outDir = Join-Path $rootDir "dist\store\$Version"
    if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
    # Tag pushes run the workflow with headBranch = the tag name.
    $runs = gh run list --workflow release.yml --limit 50 --json databaseId,headBranch,conclusion,createdAt | ConvertFrom-Json
    $run = $runs | Where-Object { $_.headBranch -eq $Version } | Sort-Object createdAt -Descending | Select-Object -First 1
    if (-not $run) { throw "No Release workflow run for $Version. Publish the GitHub release first (github-release skill), or use -Local." }
    if ($run.conclusion -ne "success") { throw "The Release run for $Version did not succeed (conclusion: '$($run.conclusion)'). Fix it before submitting to the Store." }
    # gh writes its errors to stderr, which Windows PowerShell turns into a terminating error under "Stop".
    $ErrorActionPreference = "Continue"
    $downloadLog = gh run download $run.databaseId -n "pluma-$Version-msix" -D $outDir 2>&1
    $downloaded = $LASTEXITCODE -eq 0
    $ErrorActionPreference = "Stop"
    if (-not $downloaded) {
        throw "The run $($run.databaseId) has no artifact pluma-$Version-msix (releases before the MSIX step have none: use -Local). gh: $downloadLog"
    }
    $msixPath = Join-Path $outDir $msixName
}
if (-not (Test-Path $msixPath)) { throw "Package not found: $msixPath" }

# Check the manifest inside the package against the Partner Center identity.
$identity = Get-Content (Join-Path $rootDir "packaging\msix\identity.json") -Raw -Encoding UTF8 | ConvertFrom-Json
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($msixPath)
try {
    $entry = $zip.Entries | Where-Object { $_.FullName -eq "AppxManifest.xml" }
    $reader = New-Object System.IO.StreamReader($entry.Open())
    [xml]$manifest = $reader.ReadToEnd()
    $reader.Dispose()
} finally {
    $zip.Dispose()
}
$id = $manifest.Package.Identity
$problems = @()
if ($id.Name -ne $identity.identityName) { $problems += "Identity Name '$($id.Name)' <> '$($identity.identityName)'" }
if ($id.Publisher -ne $identity.publisher) { $problems += "Publisher '$($id.Publisher)' <> '$($identity.publisher)'" }
if ($id.Version -ne $packageVersion) { $problems += "Version '$($id.Version)' <> '$packageVersion'" }
if ($manifest.Package.Properties.DisplayName -ne $identity.displayName) { $problems += "DisplayName '$($manifest.Package.Properties.DisplayName)' <> '$($identity.displayName)'" }
if ($problems) { throw "The package does not match Partner Center:`n  " + ($problems -join "`n  ") }

$hash = (Get-FileHash $msixPath -Algorithm SHA256).Hash.ToLower()
Write-Host "[OK] $msixPath"
Write-Host "     $($id.Name) $($id.Version) $($id.ProcessorArchitecture) - $([math]::Round((Get-Item $msixPath).Length / 1KB)) KB"
Write-Host "     SHA-256 $hash"
