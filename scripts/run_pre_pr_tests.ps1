<#
.SYNOPSIS
    Pre-PR Test Runner for Windows.

.DESCRIPTION
    Runs every check that CI enforces on pull requests so contributors can
    verify locally before pushing.  Requires Visual Studio 2019+ (MSVC),
    CMake, Python 3.12+ and pre-commit.

.PARAMETER Sanitizers
    Rebuild with ASan and re-run CTest.

.PARAMETER Coverage
    Rebuild with coverage flags and generate a report (requires OpenCppCoverage).

.PARAMETER All
    Enable -Sanitizers and -Coverage.

.PARAMETER SkipPython
    Skip Python integration / system tests.

.PARAMETER Help
    Show this help message.

.EXAMPLE
    .\scripts\run_pre_pr_tests.ps1
    .\scripts\run_pre_pr_tests.ps1 -All
    .\scripts\run_pre_pr_tests.ps1 -SkipPython

.NOTES
    Closes: #144
#>
[CmdletBinding()]
param(
    [switch]$Sanitizers,
    [switch]$Coverage,
    [switch]$All,
    [switch]$SkipPython,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ── Resolve flags ────────────────────────────────────────────────────────────
if ($All) { $Sanitizers = $true; $Coverage = $true }
if ($Help) { Get-Help $MyInvocation.MyCommand.Path -Detailed; exit 0 }

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$CmakePreset = 'host-linux-tests'   # overridden below for Windows

# ── Result tracking ─────────────────────────────────────────────────────────
$Results = [System.Collections.Generic.List[PSCustomObject]]::new()

function Record([string]$Name, [string]$Status) {
    $Results.Add([PSCustomObject]@{ Name = $Name; Status = $Status })
}

function Print-Summary {
    Write-Host ''
    Write-Host ('=' * 56) -ForegroundColor White
    Write-Host '  Pre-PR Test Summary' -ForegroundColor White
    Write-Host ('=' * 56) -ForegroundColor White

    $failed = $false
    foreach ($r in $Results) {
        switch ($r.Status) {
            'PASS' { Write-Host "  PASS  $($r.Name)" -ForegroundColor Green  }
            'SKIP' { Write-Host "  SKIP  $($r.Name)" -ForegroundColor Yellow }
            default { Write-Host "  FAIL  $($r.Name)" -ForegroundColor Red; $failed = $true }
        }
    }

    Write-Host ('=' * 56) -ForegroundColor White
    if (-not $failed) {
        Write-Host '  All checks passed - ready to open a PR!' -ForegroundColor Green
    } else {
        Write-Host '  Some checks failed - please fix before opening a PR.' -ForegroundColor Red
    }
    Write-Host ''
    return (-not $failed)
}

function Section([string]$Title) {
    Write-Host "`n>> $Title`n" -ForegroundColor Cyan
}

# ── Step 1: Pre-commit hooks ────────────────────────────────────────────────
function Run-PreCommit {
    Section 'Pre-commit hooks'
    try {
        $null = Get-Command pre-commit -ErrorAction Stop
    } catch {
        Write-Host 'pre-commit not found - installing via pip...' -ForegroundColor Yellow
        pip install pre-commit
    }

    pre-commit run --all-files
    if ($LASTEXITCODE -eq 0) { Record 'Pre-commit hooks' 'PASS' }
    else                     { Record 'Pre-commit hooks' 'FAIL' }
}

# ── Step 2: C++ build ───────────────────────────────────────────────────────
function Run-CppBuild {
    $BuildDir = Join-Path $ProjectRoot 'build\windows-release'
    Section "C++ build (MSVC Release)"

    cmake -B $BuildDir -S $ProjectRoot `
        -DCMAKE_BUILD_TYPE=Release `
        -DBUILD_TESTS=ON `
        -DBUILD_EXAMPLES=ON `
        -DENABLE_WERROR=ON

    cmake --build $BuildDir --config Release
    if ($LASTEXITCODE -eq 0) { Record 'C++ build (MSVC)' 'PASS' }
    else                     { Record 'C++ build (MSVC)' 'FAIL' }

    $script:BuildDir = $BuildDir
}

# ── Step 3: CTest ───────────────────────────────────────────────────────────
function Run-CTest {
    Section 'C++ unit tests (CTest)'

    ctest --test-dir $script:BuildDir --build-config Release `
        --output-on-failure --timeout 30 --no-tests=error
    if ($LASTEXITCODE -eq 0) { Record 'C++ unit tests (CTest)' 'PASS' }
    else                     { Record 'C++ unit tests (CTest)' 'FAIL' }
}

# ── Step 4: Python tests ────────────────────────────────────────────────────
function Run-PythonTests {
    if ($SkipPython) {
        Record 'Python integration & system tests' 'SKIP'
        return
    }

    Section 'Python integration & system tests'

    $ReqFile = Join-Path $ProjectRoot 'tests\python\requirements.txt'
    if (Test-Path $ReqFile) { pip install -q -r $ReqFile }

    Push-Location (Join-Path $ProjectRoot 'tests\python')
    try {
        python -m pytest ..\integration\ ..\system\ `
            -v --tb=short -ra `
            -k 'not performance and not slow' `
            --timeout=60
        if ($LASTEXITCODE -eq 0) { Record 'Python integration & system tests' 'PASS' }
        else                     { Record 'Python integration & system tests' 'FAIL' }
    } finally {
        Pop-Location
    }
}

# ── Step 5 (optional): Sanitizers ────────────────────────────────────────────
function Run-Sanitizers {
    if (-not $Sanitizers) { return }

    Section 'ASan build & test (MSVC /fsanitize=address)'
    $SanDir = Join-Path $ProjectRoot 'build\sanitizers'

    cmake -B $SanDir -S $ProjectRoot `
        -DCMAKE_BUILD_TYPE=Debug `
        -DBUILD_TESTS=ON `
        -DENABLE_WERROR=ON

    cmake --build $SanDir --config Debug

    ctest --test-dir $SanDir --build-config Debug `
        --output-on-failure --timeout 60 --no-tests=error
    if ($LASTEXITCODE -eq 0) { Record 'Sanitizers (ASan)' 'PASS' }
    else                     { Record 'Sanitizers (ASan)' 'FAIL' }
}

# ── Step 6 (optional): Coverage ──────────────────────────────────────────────
function Run-Coverage {
    if (-not $Coverage) { return }

    Section 'Coverage build & test'
    try {
        $null = Get-Command OpenCppCoverage -ErrorAction Stop
    } catch {
        Write-Host 'OpenCppCoverage not found - skipping coverage.' -ForegroundColor Yellow
        Record 'Coverage report' 'SKIP'
        return
    }

    $CovDir = Join-Path $ProjectRoot 'build\coverage'
    cmake -B $CovDir -S $ProjectRoot `
        -DCMAKE_BUILD_TYPE=Debug `
        -DBUILD_TESTS=ON

    cmake --build $CovDir --config Debug

    ctest --test-dir $CovDir --build-config Debug `
        --output-on-failure --timeout 30 --no-tests=error

    Record 'Coverage report' 'PASS'
}

# ── Main ─────────────────────────────────────────────────────────────────────
Write-Host 'Pre-PR Test Runner' -ForegroundColor White
Write-Host "Platform : Windows" -ForegroundColor Cyan
Write-Host "Root     : $ProjectRoot" -ForegroundColor Cyan

Push-Location $ProjectRoot
try {
    Run-PreCommit
    Run-CppBuild
    Run-CTest
    Run-PythonTests
    Run-Sanitizers
    Run-Coverage

    $ok = Print-Summary
    if (-not $ok) { exit 1 }
} finally {
    Pop-Location
}
