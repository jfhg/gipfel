#!/usr/bin/env bash
# Build the wipfel Android wrapper: wasm site -> assets -> debug APK.
# Requires: wasm32 target + wasm-bindgen (cargo), Android SDK, JDK 17+.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/.." && pwd)"

export ANDROID_HOME="${ANDROID_HOME:-$HOME/android-sdk}"
export PATH="$PATH:$HOME/.cargo/bin"

echo "==> building wipfel release site"
( cd "$repo/crates/wipfel" && ./build-release.sh >/dev/null )

echo "==> staging assets"
mkdir -p "$here/app/src/main/assets/www"
rm -f "$here/app/src/main/assets/www/"*
cp "$repo/dist/wipfel/"* "$here/app/src/main/assets/www/"

echo "==> gradle assembleDebug"
( cd "$here" && ./gradlew assembleDebug --no-daemon -q )

apk="$here/app/build/outputs/apk/debug/app-debug.apk"
echo "==> done: $apk ($(stat -c%s "$apk") bytes)"
echo "    install: adb install -r $apk"
