# Build FIASCO with MSVC for Windows
# Usage: .\build.ps1 [-Clean] [-Debug] [-NoPython]

param(
    [switch]$Clean,
    [switch]$Debug,
    [switch]$NoPython
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"
$InstallDir = Join-Path $ScriptDir "install"

Write-Host "=== FIASCO Build Script ===" -ForegroundColor Cyan
Write-Host "Build directory: $BuildDir"
Write-Host "Install directory: $InstallDir"

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Determine build type
$BuildType = if ($Debug) { "Debug" } else { "Release" }
Write-Host "Build type: $BuildType" -ForegroundColor Green

# Find Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) {
        Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Green
    }
}

# Configure with CMake
Write-Host "`nConfiguring with CMake..." -ForegroundColor Cyan
Push-Location $BuildDir

$cmakeArgs = @(
    "..",
    "-G", "Visual Studio 18 2026",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_INSTALL_PREFIX=$InstallDir"
)

# Check for Python
if (!$NoPython) {
    $pythonPath = (Get-Command python -ErrorAction SilentlyContinue).Source
    if ($pythonPath) {
        Write-Host "Python found: $pythonPath" -ForegroundColor Green
        
        # Check for pybind11
        $hasPybind = python -c "import pybind11; print('yes')" 2>$null
        if ($hasPybind -eq "yes") {
            Write-Host "pybind11 found" -ForegroundColor Green
        } else {
            Write-Host "pybind11 not found, installing..." -ForegroundColor Yellow
            pip install pybind11
        }
    } else {
        Write-Host "Python not found, skipping Python bindings" -ForegroundColor Yellow
    }
}

# Run CMake configure
& cmake $cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "CMake configuration failed"
}

# Build
Write-Host "`nBuilding..." -ForegroundColor Cyan
& cmake --build . --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Build failed"
}

# Install
Write-Host "`nInstalling..." -ForegroundColor Cyan
& cmake --install . --config $BuildType
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Install failed"
}

Pop-Location

# Copy Python module if built
$pyModule = Get-ChildItem -Path $BuildDir -Recurse -Filter "pyfiasco*.pyd" | Select-Object -First 1
if ($pyModule) {
    $destDir = Join-Path $ScriptDir ".." ".." "fractal_rag" "fiasco"
    if (!(Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir | Out-Null
    }
    Copy-Item $pyModule.FullName -Destination $destDir
    Write-Host "Python module copied to: $destDir" -ForegroundColor Green
}

Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "Executables: $InstallDir\bin"
Write-Host "Libraries: $InstallDir\lib"

# Show what was built
Write-Host "`nBuilt files:" -ForegroundColor Cyan
Get-ChildItem -Path $InstallDir -Recurse -File | ForEach-Object {
    Write-Host "  $($_.FullName.Replace($InstallDir, ''))"
}
