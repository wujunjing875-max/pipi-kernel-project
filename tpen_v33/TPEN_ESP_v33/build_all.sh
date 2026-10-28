#!/usr/bin/env bash
# build_all.sh —— 一键编译采集端(C) + 绘制端(Java→dex)
# 换电脑只需改下面 4 个路径（或用环境变量覆盖）
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"

NDK="${NDK:-/c/Android/sdk/ndk/26.1.10909125}"
JAVAC="${JAVAC:-/c/Program Files/Microsoft/jdk-17.0.20.101-hotspot/bin/javac}"
ANDROID_JAR="${ANDROID_JAR:-/c/Android/sdk/platforms/android-35/android.jar}"
D8="${D8:-/c/Android/sdk/build-tools/34.0.0/d8.bat}"

BIN="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe"
SYS="$NDK/toolchains/llvm/prebuilt/windows-x86_64/sysroot"

[ -x "$BIN" ] || { echo "[!] clang 不存在: $BIN（改 NDK= 路径）"; exit 1; }
[ -x "$JAVAC" ] || { echo "[!] javac 不存在: $JAVAC（改 JAVAC= 路径）"; exit 1; }
[ -f "$D8" ] || { echo "[!] d8 不存在: $D8（改 D8= 路径）"; exit 1; }

mkdir -p "$ROOT/out" "$ROOT/out/tmp"

echo "[1/2] 编译采集端 ..."
TEMP="$ROOT/out/tmp" TMP="$ROOT/out/tmp" TMPDIR="$ROOT/out/tmp" "$BIN" \
  --target=aarch64-linux-android21 -O2 -fPIE -pie \
  --sysroot="$SYS" \
  -o "$ROOT/out/p" \
  "$ROOT/src/C/paradise_fast100.c" -lm
echo "    → out/p  ($(stat -c %s "$ROOT/out/p") 字节)"

echo "[2/2] 编译绘制端 ..."
rm -rf "$ROOT/out/classes" "$ROOT/out/dex"; mkdir -p "$ROOT/out/classes" "$ROOT/out/dex"
"$JAVAC" -nowarn -encoding UTF-8 --release 8 \
  -classpath "$ANDROID_JAR" -d "$ROOT/out/classes" "$ROOT/src/Java/Ov50.java"
"$D8" --lib "$ANDROID_JAR" --release --min-api 26 \
  --output "$ROOT/out/dex" "$ROOT/out/classes/Ov50.class"
mv "$ROOT/out/dex/classes.dex" "$ROOT/out/classes.dex"
echo "    → out/classes.dex  ($(stat -c %s "$ROOT/out/classes.dex") 字节)"

echo "完成。产物在 out/：p + classes.dex"
