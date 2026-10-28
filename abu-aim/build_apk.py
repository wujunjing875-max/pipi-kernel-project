# -*- coding: utf-8 -*-
"""无 Gradle 手工构建 APK：aapt2 -> javac -> d8 -> zip -> zipalign -> apksigner"""
import os, subprocess, sys, shutil, zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.join(ROOT, "sdk")
BT = os.path.join(SDK, "bt", "android-14")
ANDROID_JAR = os.path.join(SDK, "plat", "android-36", "android.jar")
APP = os.path.join(ROOT, "app")
OUT = os.path.join(ROOT, "out")
PKG = "com.jianshang.aim"
NAME = "自瞄开关"
VERSION = "1.0"

def run(cmd, cwd=None, shell=False, check=True):
    print(">>", " ".join(cmd) if isinstance(cmd, list) else cmd)
    p = subprocess.run(cmd, cwd=cwd, shell=shell, capture_output=True)
    o = (p.stdout or b"").decode("utf-8", "ignore")
    e = (p.stderr or b"").decode("utf-8", "ignore")
    if o.strip(): print(o[-3000:])
    if p.returncode != 0:
        print("!! STDERR:", e[-3000:])
        if check: sys.exit(p.returncode)
    return p

def main():
    os.makedirs(OUT, exist_ok=True)
    gen = os.path.join(OUT, "gen"); cls = os.path.join(OUT, "classes")
    dex = os.path.join(OUT, "dex")
    for d in (gen, cls, dex):
        shutil.rmtree(d, ignore_errors=True); os.makedirs(d, exist_ok=True)

    aapt2 = os.path.join(BT, "aapt2.exe")
    zipalign = os.path.join(BT, "zipalign.exe")
    d8 = os.path.join(BT, "d8.bat")
    apksigner = os.path.join(BT, "apksigner.bat")

    # 1 编译资源
    flat = os.path.join(OUT, "res.zip")
    run([aapt2, "compile", "--dir", os.path.join(APP, "res"), "-o", flat])

    # 2 链接资源 + 生成 R.java
    base = os.path.join(OUT, "base.apk")
    run([aapt2, "link", "-o", base, "-I", ANDROID_JAR,
         "--manifest", os.path.join(APP, "AndroidManifest.xml"),
         "-R", flat, "--java", gen,
         "--min-sdk-version", "28", "--target-sdk-version", "28",
         "--auto-add-overlay"])

    # 3 javac
    srcs = []
    for r, _, fs in os.walk(os.path.join(APP, "java")):
        srcs += [os.path.join(r, f) for f in fs if f.endswith(".java")]
    for r, _, fs in os.walk(gen):
        srcs += [os.path.join(r, f) for f in fs if f.endswith(".java")]
    run(["javac", "-encoding", "UTF-8", "-source", "8", "-target", "8",
         "-nowarn", "-classpath", ANDROID_JAR, "-d", cls] + srcs)

    # 4 d8 -> classes.dex
    classfiles = []
    for r, _, fs in os.walk(cls):
        classfiles += [os.path.join(r, f) for f in fs if f.endswith(".class")]
    run([d8, "--release", "--min-api", "28", "--lib", ANDROID_JAR,
         "--output", dex] + classfiles, shell=True)

    # 5 把 classes.dex 塞进 apk
    unsigned = os.path.join(OUT, "unsigned.apk")
    shutil.copy(base, unsigned)
    with zipfile.ZipFile(unsigned, "a", zipfile.ZIP_DEFLATED) as z:
        z.write(os.path.join(dex, "classes.dex"), "classes.dex")

    # 6 zipalign
    aligned = os.path.join(OUT, "aligned.apk")
    run([zipalign, "-f", "-p", "4", unsigned, aligned])

    # 7 签名
    ks = os.path.join(OUT, "js.keystore")
    if not os.path.exists(ks):
        run(["keytool", "-genkeypair", "-v", "-keystore", ks, "-alias", "js",
             "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
             "-storepass", "android", "-keypass", "android",
             "-dname", "CN=JianShang, OU=JS, O=JS, L=CN, ST=CN, C=CN"])
    final = os.path.join(OUT, NAME + "_v" + VERSION + ".apk")
    run([apksigner, "sign", "--ks", ks, "--ks-pass", "pass:android",
         "--key-pass", "pass:android", "--out", final, aligned], shell=True)
    run([apksigner, "verify", "--print-certs", final], shell=True, check=False)
    print("\n== 产物:", final, os.path.getsize(final), "bytes")

if __name__ == "__main__":
    main()
