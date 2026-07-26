# Android Build

This branch contains the experimental Android port of TrenchBroom. The tested
configuration targets 64-bit ARM Android devices.

## Requirements

- Qt 6.7.3 for `android_arm64_v8a`
- A matching Qt 6.7.3 Windows host installation
- Android SDK platform 34 and build tools 34.0.0
- Android NDK 26.1.10909125
- CMake, Ninja, JDK 17, and Git

Initialize the vcpkg submodule after cloning:

```powershell
git submodule update --init --recursive
```

## Configure

Set these paths for your machine:

```powershell
$QtAndroid = "C:/Qt/6.7.3/android_arm64_v8a"
$QtHost = "C:/Qt/6.7.3/mingw_64"
$SdkRoot = "C:/Android/Sdk"
$NdkRoot = "$SdkRoot/ndk/26.1.10909125"
```

Configure the project:

```powershell
cmake -S . -B build-android-arm64 -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$QtAndroid/lib/cmake/Qt6/qt.toolchain.cmake" `
  -DVCPKG_TARGET_TRIPLET=arm64-android `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-34 `
  -DANDROID_SDK_ROOT="$SdkRoot" `
  -DANDROID_NDK_ROOT="$NdkRoot" `
  -DCMAKE_PREFIX_PATH="$QtAndroid" `
  -DQT_HOST_PATH="$QtHost"
```

## Build

```powershell
cmake --build build-android-arm64 --target TrenchBroom_make_apk -j 4
```

The debug APK is written below the build directory at:

```text
app/TrenchBroom/android-build/build/outputs/apk/debug/android-build-debug.apk
```

Install it on a connected device with:

```powershell
adb install -r path/to/android-build-debug.apk
```
