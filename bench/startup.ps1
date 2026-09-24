param (
    [string]$ExePath = "$PSScriptRoot\..\build\release\bin\pluma.exe",
    [int]$Iterations = 20,
    [int]$Warmup = 2,
    [double]$TargetLimitMs = 60.0,
    [double]$HardLimitMs = 100.0,
    [switch]$FailOnHardLimit
)

$resolvedExe = Resolve-Path $ExePath -ErrorAction SilentlyContinue
if (-not $resolvedExe -or -not (Test-Path $resolvedExe)) {
    Write-Error "Executable not found at '$ExePath'"
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Pluma Startup Benchmark (NF-01 / M0.5)" -ForegroundColor Cyan
Write-Host " Executable: $resolvedExe" -ForegroundColor Cyan
Write-Host " Iterations: $Iterations (Warmup: $Warmup)" -ForegroundColor Cyan
Write-Host " Budget: Target <= ${TargetLimitMs}ms | Hard limit <= ${HardLimitMs}ms" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

function Measure-SingleStartup {
    param([string]$path)

    # Temporary dummy event handle name generated before launch,
    # or we create a process suspended / detect PID and listen.
    # To avoid race conditions, we can start process and wait on event with PID.
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $path
    $psi.Arguments = ""
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $false

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $proc = [System.Diagnostics.Process]::Start($psi)
    if (-not $proc) {
        throw "Failed to start process."
    }

    $eventName = "Local\PlumaStartupEvent_" + $proc.Id
    $eventHandle = $null
    $createdNew = $false
    # Create the named event if not exists so when the app opens it, it will find it
    $eventHandle = New-Object System.Threading.EventWaitHandle($false, [System.Threading.EventResetMode]::ManualReset, $eventName, [ref]$createdNew)

    # Wait up to 5 seconds for the first paint signal
    $signaled = $eventHandle.WaitOne(5000)
    $sw.Stop()

    $elapsedMs = $sw.Elapsed.TotalMilliseconds

    # Cleanup
    if (-not $proc.HasExited) {
        $proc.Kill()
        $proc.WaitForExit(1000)
    }
    $eventHandle.Close()

    if (-not $signaled) {
        Write-Warning "Process $($proc.Id) did not signal startup event within 5000ms."
        return $null
    }

    return $elapsedMs
}

# Warmup runs
for ($i = 1; $i -le $Warmup; $i++) {
    $null = Measure-SingleStartup -path $resolvedExe
}

# Measurement runs
$timings = @()
for ($i = 1; $i -le $Iterations; $i++) {
    $t = Measure-SingleStartup -path $resolvedExe
    if ($null -ne $t) {
        $timings += $t
        Write-Host "  Run $i/$Iterations: $([math]::Round($t, 2)) ms"
    }
    Start-Sleep -Milliseconds 100
}

if ($timings.Count -eq 0) {
    Write-Error "No valid timings collected."
    exit 1
}

$sorted = $timings | Sort-Object
$count = $sorted.Count
$min = $sorted[0]
$max = $sorted[-1]
$avg = ($sorted | Measure-Object -Average).Average

# Median
if ($count % 2 -eq 1) {
    $median = $sorted[[math]::Floor($count / 2)]
} else {
    $median = ($sorted[$count / 2 - 1] + $sorted[$count / 2]) / 2.0
}

# P95 (95th percentile)
$p95Index = [math]::Ceiling($count * 0.95) - 1
if ($p95Index -ge $count) { $p95Index = $count - 1 }
$p95 = $sorted[$p95Index]

Write-Host "----------------------------------------------------------"
Write-Host "Results ($count valid runs):" -ForegroundColor Green
Write-Host "  Min:    $([math]::Round($min, 2)) ms"
Write-Host "  Avg:    $([math]::Round($avg, 2)) ms"
Write-Host "  Median: $([math]::Round($median, 2)) ms" -ForegroundColor $(if ($median -le $TargetLimitMs) { "Green" } else { "Yellow" })
Write-Host "  P95:    $([math]::Round($p95, 2)) ms" -ForegroundColor $(if ($p95 -le $HardLimitMs) { "Green" } else { "Red" })
Write-Host "  Max:    $([math]::Round($max, 2)) ms"
Write-Host "=========================================================="

if ($p95 -gt $HardLimitMs) {
    Write-Host "[FAIL] P95 ($([math]::Round($p95, 2)) ms) exceeds hard limit of ${HardLimitMs} ms!" -ForegroundColor Red
    if ($FailOnHardLimit) {
        exit 1
    }
} else {
    Write-Host "[PASS] Startup time is within hard limit (${HardLimitMs} ms)." -ForegroundColor Green
}
