import base64
import hashlib
import os
import shutil
import textwrap
import zipfile
from pathlib import Path

ROOT = Path(r"F:\CodexWork\tpen-esp\tpen_v33\TPEN_ESP_v33")
ROLE = Path(r"F:\CodexWork\tpen-esp\outputs\tpen_esp_opd2413_matched")
MOD = ROLE / "MODIFIED_FILE"
PACKAGE = ROLE / "package"
ZIP_PATH = ROLE / "tpen_esp_opd2413_matched.zip"


def block(tag: str, path: Path, end_prefix: str) -> str:
    encoded = base64.b64encode(path.read_bytes()).decode("ascii")
    rows = "\n".join(textwrap.wrap(encoded, 76))
    suffix = tag.split("_", 1)[1]
    return f"__{tag}__\n{rows}\n__{end_prefix}{suffix}__\n"


def write_script(path: Path, header: str, blocks: list[tuple[str, Path, str]]) -> None:
    body = header.rstrip() + "\n"
    for tag, source, end_prefix in blocks:
        body += block(tag, source, end_prefix)
    path.write_text(body, encoding="ascii", newline="\n")
    os.chmod(path, 0o755)


def main() -> None:
    PACKAGE.mkdir(parents=True, exist_ok=True)
    driver_header = r'''#!/system/bin/sh
set -eu
[ "$(id -u)" = "0" ] || { echo "run driver.sh as root" >&2; exit 1; }
KV=${1:-66}
D=/data/local/tmp
T="$D/.tpen_drv_$$.sh"
trap 'rm -f "$T"' EXIT INT TERM
case "$KV" in 510|515|61|612|618|66) ;; *) echo "usage: sh driver.sh [510|515|61|612|618|66]" >&2; exit 2;; esac
awk "/^__DRV_${KV}__/{f=1;next}/^__END_DRV_${KV}__/{exit}f" "$0" | base64 -d > "$T"
chmod 700 "$T"
sh "$T"
echo "driver loaded: $KV"
exit 0
'''
    driver_blocks = [(f"DRV_{kv}", ROOT / "drivers" / f"drv_{kv}.sh", "END_DRV_")
                     for kv in ("510", "515", "61", "612", "618", "66")]
    write_script(PACKAGE / "driver.sh", driver_header, driver_blocks)

    process_header = r'''#!/system/bin/sh
set -eu
[ "$(id -u)" = "0" ] || { echo "run process.sh as root" >&2; exit 1; }
D=/data/local/tmp
C="$D/.c"
U="$D/.tpen_unpack_$$"
mkdir -p "$U"
trap 'rm -rf "$U"' EXIT INT TERM
extract(){ awk "/^__PAYLOAD_$1__/{f=1;next}/^__END_PAYLOAD_$1__/{exit}f" "$0" | base64 -d > "$U/$2"; }
extract P p_latest
extract DEX ov_new.dex
extract GPNG glyph.png
extract GTXT glyph.txt
extract UIPNG ov_ui.png
extract UITXT ov_ui.txt
extract CTYPE ov_ctype.txt
extract LEVEL ov_level.txt
extract NAME ov_name.txt
extract PRICE ov_price.txt
extract SKIP ov_skip.txt
for d in /proc/[0-9]*; do
  e=$(readlink "$d/exe" 2>/dev/null || true)
  case "$e" in */.c/p*) kill -9 "${d#/proc/}" 2>/dev/null || true;; esac
done
pkill -9 -f Ov50 2>/dev/null || true
sleep 1
chmod -R 777 "$C" 2>/dev/null || true
rm -rf "$C"
mkdir -p "$C"
chmod 777 "$C"
cp "$U/p_latest" "$C/p"
cp "$U/ov_new.dex" "$C/o.dex"
chmod 755 "$C/p"
chmod 644 "$C/o.dex"
cp "$U/glyph.png" "$D/glyph.png"
cp "$U/glyph.txt" "$D/glyph.txt"
cp "$U/ov_ui.png" "$D/ov_ui.png"
cp "$U/ov_ui.txt" "$D/ov_ui.txt"
cp "$U/ov_ctype.txt" "$D/ov_ctype.txt"
cp "$U/ov_level.txt" "$D/ov_level.txt"
cp "$U/ov_name.txt" "$D/ov_name.txt"
cp "$U/ov_price.txt" "$D/ov_price.txt"
cp "$U/ov_skip.txt" "$D/ov_skip.txt"
# OPD2413 fixed touch range: normalize only calibration and keep user toggles.
CFG="$D/ov_cfg.txt"
CFGT="$D/.tpen_cfg_$$"
if [ -f "$CFG" ]; then
  grep -v -E '^(screen|rot|aff)=' "$CFG" > "$CFGT" || true
else
  : > "$CFGT"
fi
cat >> "$CFGT" <<'EOF_TPEN_CFG'
screen=3040x1904
rot=0
aff=0.000000000,0.100000000,0.000000000,0.100000000,0.000000000,0.000000000
EOF_TPEN_CFG
mv "$CFGT" "$CFG"
chmod 644 "$CFG"
cfg_bool(){
  v=$(sed -n "s/^$1=//p" "$CFG" | tail -n 1)
  case "$v" in 0|1) printf '%s\n' "$v";; *) printf '%s\n' "$2";; esac
}
cfg_bool dec 1 > "$D/ov_dec.txt"
cfg_bool dec2 0 > "$D/ov_dec2.txt"
cfg_bool dec3 0 > "$D/ov_dec3.txt"
cfg_bool dec4 0 > "$D/ov_dec4.txt"
chmod 666 "$D"/ov_dec*.txt
setsid sh -c "exec $C/p > $C/p.log 2>&1" < /dev/null &
sleep 2
cd "$D"
setsid sh -c "CLASSPATH=$C/o.dex exec app_process $D Ov50 3040 1904 0 0 > $C/o.log 2>&1" < /dev/null &
echo "process started: 3040x1904, waiting for com.tencent.mf.uam"
exit 0
'''
    payloads = [
        ("PAYLOAD_P", MOD / "out" / "p", "END_PAYLOAD_"),
        ("PAYLOAD_DEX", MOD / "out" / "classes.dex", "END_PAYLOAD_"),
        ("PAYLOAD_GPNG", ROOT / "assets" / "g.png", "END_PAYLOAD_"),
        ("PAYLOAD_GTXT", ROOT / "assets" / "g.txt", "END_PAYLOAD_"),
        ("PAYLOAD_UIPNG", ROOT / "assets" / "ui.png", "END_PAYLOAD_"),
        ("PAYLOAD_UITXT", ROOT / "assets" / "ui.txt", "END_PAYLOAD_"),
        ("PAYLOAD_CTYPE", ROOT / "assets" / "ctype.txt", "END_PAYLOAD_"),
        ("PAYLOAD_LEVEL", ROOT / "assets" / "level.txt", "END_PAYLOAD_"),
        ("PAYLOAD_NAME", ROOT / "assets" / "name.txt", "END_PAYLOAD_"),
        ("PAYLOAD_PRICE", ROOT / "assets" / "price.txt", "END_PAYLOAD_"),
        ("PAYLOAD_SKIP", ROOT / "assets" / "skip.txt", "END_PAYLOAD_"),
    ]
    write_script(PACKAGE / "process.sh", process_header, payloads)

    if ZIP_PATH.exists():
        ZIP_PATH.unlink()
    with zipfile.ZipFile(ZIP_PATH, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for name in ("driver.sh", "process.sh"):
            source = PACKAGE / name
            info = zipfile.ZipInfo(name)
            info.date_time = (2026, 10, 30, 0, 0, 0)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100755 << 16
            zf.writestr(info, source.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)

    for path in (PACKAGE / "driver.sh", PACKAGE / "process.sh", ZIP_PATH):
        print(f"{path}\t{path.stat().st_size}\t{hashlib.sha256(path.read_bytes()).hexdigest().upper()}")


if __name__ == "__main__":
    main()
