param(
  [Parameter(Mandatory = $true)]
  [string] $QtAndroidRoot,

  [string] $Abi = "arm64-v8a",
  [string] $AndroidPlatform = "android-26",
  [string] $BuildType = "Debug",
  [string] $BuildDir = "build-android-arm64"
)

$ErrorActionPreference = "Stop"

$qtCmake = Join-Path $QtAndroidRoot "bin\qt-cmake.bat"
if (-not (Test-Path -LiteralPath $qtCmake)) {
  throw "qt-cmake.bat was not found under '$QtAndroidRoot'. Pass a Qt Android ABI root such as C:\Qt\6.10.0\android_arm64_v8a."
}

$sourceDir = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $sourceDir $BuildDir

& $qtCmake `
  -S $sourceDir `
  -B $buildPath `
  -G Ninja `
  -DANDROID_ABI=$Abi `
  -DANDROID_PLATFORM=$AndroidPlatform `
  -DCMAKE_BUILD_TYPE=$BuildType

& cmake --build $buildPath --target TrenchBroom
