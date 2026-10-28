# -*- coding: utf-8 -*-
import zipfile, os

base = os.path.dirname(os.path.abspath(__file__))
res = os.path.join(base, 'out', 'resources.apk')
dex = os.path.join(base, 'out', 'dex', 'classes.dex')
assets = os.path.join(base, 'assets')
unsigned = os.path.join(base, 'out', 'unsigned.apk')

zin = zipfile.ZipFile(res, 'r')
zout = zipfile.ZipFile(unsigned, 'w', zipfile.ZIP_DEFLATED)
for it in zin.infolist():
    if it.filename == 'classes.dex':
        continue
    zout.writestr(it, zin.read(it.filename))
zin.close()

zout.write(dex, 'classes.dex')

# assets 用 STORED（不压缩），保证大文件可被 AssetManager 正常读取
for f in sorted(os.listdir(assets)):
    p = os.path.join(assets, f)
    if os.path.isfile(p):
        zi = zipfile.ZipInfo('assets/' + f)
        zi.compress_type = zipfile.ZIP_STORED
        zi.external_attr = 0o644 << 16
        with open(p, 'rb') as fh:
            zout.writestr(zi, fh.read())

# walls 子目录（掩体碰撞表）：用 DEFLATED 压缩，AssetManager.open 会自动解压
wdir = os.path.join(assets, 'walls')
if os.path.isdir(wdir):
    for f in sorted(os.listdir(wdir)):
        p = os.path.join(wdir, f)
        if os.path.isfile(p):
            zi = zipfile.ZipInfo('assets/walls/' + f)
            zi.compress_type = zipfile.ZIP_DEFLATED
            zi.external_attr = 0o644 << 16
            with open(p, 'rb') as fh:
                zout.writestr(zi, fh.read())
zout.close()
print("unsigned.apk:", os.path.getsize(unsigned), "bytes")
