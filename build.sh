#!/bin/bash
# ------------------------------------------------------------
# FanoMonitor Build Script
#
# Built by:     IamCOD3X
# GitHub:       https://www.github.com/IamCOD3X
# Website:      https://iamcod3x.com
# Date:         September 27, 2025
#
# Description:
#   Cross-compiles the fanomonitor binary for all Android ABIs
#   using the Android NDK (Clang toolchain).
# ------------------------------------------------------------
set -e

NDK_HOME="/home/ripper/Android/Sdk/ndk/ndk-28"
SRC="fanomonitor.c"
OUT_DIR="fanomonitor_out"
API=21   # minimal Android version
ARCHS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")

mkdir -p "$OUT_DIR"

echo "🔨 Building $SRC as standalone binary for Android..."

for ARCH in "${ARCHS[@]}"; do
    case $ARCH in
        "arm64-v8a")
            TARGET="aarch64-linux-android${API}"
            ;;
        "armeabi-v7a")
            TARGET="armv7a-linux-androideabi${API}"
            ;;
        "x86")
            TARGET="i686-linux-android${API}"
            ;;
        "x86_64")
            TARGET="x86_64-linux-android${API}"
            ;;
    esac

    TOOL="$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/${TARGET}-clang"

    ARCH_OUT="$OUT_DIR/$ARCH"
    mkdir -p "$ARCH_OUT"

    echo "➡️  Building for $ARCH..."
    $TOOL "$SRC" -o "$ARCH_OUT/fanomonitor" \
	-Wno-implicit-function-declaration -Wno-format-extra-args \
        -Wall -O2 -llog

    echo "✅ Output: $ARCH_OUT/fanomonitor"
done

echo "🎉 All builds done. Copy fanomonitor_out/* into assets/bin/"

