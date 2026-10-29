#!/usr/bin/env bash
# Builds the Android APK.  Requires JDK 17+ and the Android SDK (ANDROID_HOME)
# with NDK 27.  On a plain Linux box:
#   export ANDROID_HOME=$HOME/Android/Sdk
#   scripts/build_apk.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT/scripts/fetch_sdl.sh"

cd "$ROOT/android"
./gradlew --no-daemon assembleDebug
echo
echo "APK: android/app/build/outputs/apk/debug/app-debug.apk"
