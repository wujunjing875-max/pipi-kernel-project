#!/bin/bash
# 只编译绘制端 Ov50.java -> dex/classes.dex（不碰采集端 C，无 NDK 也能跑）
set -e
cd "$(dirname "$0")"
BT="/c/Android/sdk/build-tools/34.0.0"
JAR="/c/Android/sdk/platforms/android-35/android.jar"

rm -rf out/ovcls out/ovdex out/ov.jar; mkdir -p out/ovcls out/ovdex

echo "[1/3] javac"
javac -nowarn -encoding UTF-8 -cp "$JAR" -d out/ovcls src/Java/Ov50.java

echo "[2/3] d8"
jar cf out/ov.jar -C out/ovcls .
java -cp "$BT/lib/d8.jar" com.android.tools.r8.D8 --lib "$JAR" --release --min-api 26 \
  --output out/ovdex out/ov.jar

echo "[3/3] 落盘"
cp -f out/ovdex/classes.dex dex/classes.dex
cp -f out/ovdex/classes.dex ../../tpen_app/assets/ov.dex
ls -la dex/classes.dex ../../tpen_app/assets/ov.dex
echo "DEX OK"
