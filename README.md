# TrenchBroom for Android

[![TrenchBroom icon](app/TrenchBroom/resources/graphics/images/AppIcon.png)](https://github.com/spinaljamin/TrenchBroom-Android)

This repository contains an experimental Android port of
[TrenchBroom](https://github.com/TrenchBroom/TrenchBroom), the cross-platform
level editor for Quake-engine games.

The Android work is maintained on the
[`android-port`](https://github.com/spinaljamin/TrenchBroom-Android/tree/android-port)
branch. It is an independent community port and is not an official TrenchBroom
release.

## Project Status

This port is an alpha intended for testing on 64-bit ARM Android devices.

| Item | Tested configuration |
| --- | --- |
| Source branch | `android-port` |
| Upstream base | [`179e25c0`](https://github.com/TrenchBroom/TrenchBroom/commit/179e25c0b53aaed4ed9f9d9e2970de6823cd24b1) |
| Android ABI | `arm64-v8a` |
| Android build platform | API 34 |
| Qt | 6.7.3 |
| NDK | 26.1.10909125 |
| Package type | Debug-signed APK |
| Input | Touchscreen, physical mouse, and keyboard |

The port currently supports:

- Creating and editing maps in the 3D and 2D views
- Selecting, drawing, moving, and resizing brushes
- Physical mouse press, drag, release, hover, and fly-camera controls
- Touchscreen drawing and dragging
- OpenGL ES rendering of geometry and textures
- Game and map-format selection
- Bundled game definitions and editor resources
- Android document pickers for browsing, opening, and saving files
- Preferences dialogs and Android file/directory selection
- Desktop-style menus adapted for Android pointer input

This is still desktop-oriented software running on Android. Some workflows,
dialogs, keyboard shortcuts, uncommon tools, games, and device configurations
have not been fully tested.

## Download

The current test build is available from:

- [TrenchBroom Android v0.1.0 Alpha release](https://github.com/spinaljamin/TrenchBroom-Android/releases/tag/android-port-v0.1.0-alpha)
- [Direct ARM64 APK download](https://github.com/spinaljamin/TrenchBroom-Android/releases/download/android-port-v0.1.0-alpha/TrenchBroom-Android-debug-arm64.apk)

The APK is debug-signed and is not distributed through Google Play. Android may
ask for permission to install applications from the browser or file manager used
to open it.

Install or update it from a computer with ADB:

```powershell
adb install -r TrenchBroom-Android-debug-arm64.apk
```

The published APK SHA-256 is:

```text
7D0D812335B37EC9B3E5CE327F80028123F83F68DD2CD4C835AA146C7FE4FE7A
```

## Controls

The editor retains TrenchBroom's desktop interaction model.

- **Left mouse button:** select, draw, and drag
- **Right mouse button:** hold to use fly-camera look
- **Mouse wheel:** zoom or move through the view where supported
- **Touch:** select, draw, and drag directly
- **Keyboard:** standard TrenchBroom shortcuts and text entry
- **Escape:** close menus/dialogs or cancel the current tool where supported

A physical mouse and keyboard are recommended for serious editing.

## Build Requirements

The known-working build was produced on Windows. Other hosts may work with the
equivalent Qt host tools, but they have not been verified for this port.

Install:

- Git
- CMake
- Ninja
- JDK 17
- Qt 6.7.3 `android_arm64_v8a`
- Qt 6.7.3 Windows host tools, such as `mingw_64`
- Android SDK command-line tools
- Android SDK platform 34
- Android build tools 34.0.0
- Android NDK 26.1.10909125
- Android platform tools for `adb`

The Qt Android and Qt host packages must use the same Qt version.

## Clone

Clone the Android branch and initialize the vcpkg submodule:

```powershell
git clone --branch android-port --recurse-submodules `
  https://github.com/spinaljamin/TrenchBroom-Android.git
cd TrenchBroom-Android
```

For an existing clone:

```powershell
git switch android-port
git submodule update --init --recursive
```

The first vcpkg dependency build can take a significant amount of time and disk
space.

## Configure

Set paths for the local Qt and Android installations. These examples use
forward slashes because they are accepted by CMake on Windows.

```powershell
$QtAndroid = "C:/Qt/6.7.3/android_arm64_v8a"
$QtHost = "C:/Qt/6.7.3/mingw_64"
$SdkRoot = "C:/Android/Sdk"
$NdkRoot = "$SdkRoot/ndk/26.1.10909125"
$SourceRoot = (Get-Location).Path.Replace("\", "/")

$env:ANDROID_SDK_ROOT = $SdkRoot
$env:ANDROID_HOME = $SdkRoot
$env:ANDROID_NDK_ROOT = $NdkRoot
$env:ANDROID_NDK_HOME = $NdkRoot
```

Configure an ARM64 debug build:

```powershell
cmake -S . -B build-android-arm64 -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_CXX_SCAN_FOR_MODULES=OFF `
  "-DCMAKE_TOOLCHAIN_FILE=$SourceRoot/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$QtAndroid/lib/cmake/Qt6/qt.toolchain.cmake" `
  -DVCPKG_TARGET_TRIPLET=arm64-android `
  -DANDROID_ABI=arm64-v8a `
  -DANDROID_PLATFORM=android-34 `
  "-DANDROID_SDK_ROOT=$SdkRoot" `
  "-DANDROID_NDK_ROOT=$NdkRoot" `
  "-DCMAKE_PREFIX_PATH=$QtAndroid" `
  "-DQT_HOST_PATH=$QtHost"
```

If Ninja is not available on `PATH`, add:

```powershell
"-DCMAKE_MAKE_PROGRAM=C:/path/to/ninja.exe"
```

## Build the APK

Set the Android environment variables again if this is a new terminal, then
build Qt's APK target:

```powershell
cmake --build build-android-arm64 `
  --target TrenchBroom_make_apk `
  -j 4
```

The APK is generated at:

```text
build-android-arm64/app/TrenchBroom/android-build/build/outputs/apk/debug/android-build-debug.apk
```

Install the local build:

```powershell
adb install -r `
  build-android-arm64/app/TrenchBroom/android-build/build/outputs/apk/debug/android-build-debug.apk
```

Launch it from Android's app drawer or with:

```powershell
adb shell monkey -p org.qtproject.example.TrenchBroom 1
```

## Troubleshooting

### CMake cannot find Qt

Check that `CMAKE_PREFIX_PATH` points to the Android Qt package and
`QT_HOST_PATH` points to the matching desktop host package.

### CMake or Gradle reports an NDK mismatch

Use NDK 26.1.10909125 or update all SDK, CMake, Qt, and environment references
to one compatible NDK version. Mixing NDK versions can produce configuration or
link errors.

### vcpkg dependencies fail

Confirm the submodule is initialized:

```powershell
git submodule update --init --recursive
```

Delete only the local `build-android-arm64` directory and configure again after
changing Qt, NDK, ABI, generator, or toolchain settings.

### ADB cannot find the device

Enable Developer options and USB debugging on Android, accept the computer's
authorization prompt, and check:

```powershell
adb devices
```

### Android refuses to install the APK

Allow installation from the application opening the APK. If a build with a
different signing key is already installed, uninstall that package first or use
the same signing configuration.

## Known Limitations

- Only `arm64-v8a` has been built and tested.
- The release APK uses a debug signing key.
- The interface is designed for a desktop-sized display.
- A mouse and keyboard provide the most complete editing experience.
- Google Play packaging, release signing, and store distribution are not set up.
- Updating to newer upstream TrenchBroom revisions will require porting and
  retesting the Android-specific changes.
- Not every game configuration, model format, tool, dialog, or shortcut has been
  tested on Android.

Please report Android-specific problems in this fork's
[issue tracker](https://github.com/spinaljamin/TrenchBroom-Android/issues).
Include the Android version, device model, input device, and steps to reproduce.

## Upstream TrenchBroom

The original project remains the authority for desktop TrenchBroom:

- [Official repository](https://github.com/TrenchBroom/TrenchBroom)
- [Website](https://trenchbroom.github.io/)
- [Official manual](https://trenchbroom.github.io/manual/latest/)
- [Discord](https://discord.gg/WGf9uve)

Android-port issues should be reported here rather than to the upstream project
unless they can also be reproduced in an official desktop build.

## License

This fork retains TrenchBroom's GNU General Public License version 3. See
[`LICENSE.txt`](LICENSE.txt). TrenchBroom and its original source remain credited
to the upstream project and its contributors.