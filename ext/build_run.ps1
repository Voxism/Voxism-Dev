$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = (Resolve-Path (Join-Path $ScriptDir "..")).Path
$Build = Join-Path $Root "build"
$Exe = Join-Path $Build "Voxism.exe"

Set-Location $ScriptDir

function Get-PreferredGenerator {
  if (Get-Command ninja -ErrorAction SilentlyContinue) {
    return "Ninja"
  }
  if (Get-Command mingw32-make -ErrorAction SilentlyContinue) {
    return "MinGW Makefiles"
  }
  return $null
}

if (-not (Test-Path (Join-Path $Build "CMakeCache.txt"))) {
  Write-Host "Configuring CMake (build/)..."
  $generator = Get-PreferredGenerator
  if ($generator) {
    cmake "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" -G $generator -S $Root -B $Build
  } else {
    cmake "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" -S $Root -B $Build
  }
}

Write-Host "Building Voxism..."
cmake --build $Build --parallel

if (-not (Test-Path $Exe)) {
  throw "Build finished but executable not found: $Exe"
}

Write-Host "Running Voxism (cwd: build/)..."
Push-Location $Build
try {
  & $Exe @args
} finally {
  Pop-Location
}
