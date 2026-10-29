#!/usr/bin/env bash
# Downloads the SDL3 sources that the Android build compiles.
# Usage: scripts/fetch_sdl.sh [tag]
set -euo pipefail

SDL_TAG="${1:-release-3.2.30}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/android/app/jni/SDL"

if [ -f "$DEST/CMakeLists.txt" ]; then
    echo "SDL3 is already in android/app/jni/SDL"
    exit 0
fi

echo "Cloning SDL3 $SDL_TAG ..."
git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL "$DEST"
echo "Done: $DEST"
