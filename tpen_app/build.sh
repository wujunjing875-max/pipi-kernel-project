#!/bin/bash
# TPEN ESP 启动器 APK 一键构建脚本
set -e
cd "$(dirname "$0")"
BT="/c/Android/sdk/build-tools/34.0.0"
JAR="/c/Android/sdk/platforms/android-35/android.jar"

rm -rf out/obj out/dex out/obj.jar out/resources.apk out/unsigned.apk out/aligned.apk
mkdir -p out/obj out/dex

echo "[1/6] aapt2 link"
"$BT/aapt2.exe" link -I "$JAR" --manifest AndroidManifest.xml \
  --min-sdk-version 29 --target-sdk-version 35 --auto-add-overlay -o out/resources.apk

echo "[2/6] javac"
javac -encoding UTF-8 -cp "$JAR" -d out/obj src/com/tpen/esp/*.java 2>&1 | grep -v "^注意" || true

echo "[3/6] jar + d8"
jar cf out/obj.jar -C out/obj .
java -cp "$BT/lib/d8.jar" com.android.tools.r8.D8 --min-api 29 --output out/dex out/obj.jar

echo "[4/6] 组装 APK"
python build_apk.py

echo "[5/6] zipalign"
"$BT/zipalign.exe" -f -p 4 out/unsigned.apk out/aligned.apk

echo "[6/6] 签名"
if [ ! -f tpen.keystore ]; then
  keytool -genkeypair -keystore tpen.keystore -alias tpen -keyalg RSA -keysize 2048 \
    -validity 10950 -storepass tpen1234 -keypass tpen1234 \
    -dname "CN=TPEN, OU=ESP, O=TPEN, L=CN, S=CN, C=CN" >/dev/null 2>&1
fi
"$BT/apksigner.bat" sign --ks tpen.keystore --ks-key-alias tpen \
  --ks-pass pass:tpen1234 --key-pass pass:tpen1234 --out out/TPEN.apk out/aligned.apk

ls -la out/TPEN.apk
echo "BUILD OK"
