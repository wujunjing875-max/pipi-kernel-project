#!/usr/bin/env bash
# deploy.sh —— 一键部署 + 启动（需已 root 的 adb 设备）
# 用法：  bash deploy.sh [KERNEL_VER]      KERNEL_VER = 61(默认) / 510 / 515 / 612 / 618 / 66
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
KV="${1:-61}"
ADB="adb"
D=/data/local/tmp
C=$D/.c

echo "[1/4] 推文件 ..."
$ADB push "$ROOT/out/p"        $D/p_latest       >/dev/null
$ADB push "$ROOT/out/classes.dex" $D/ov_new.dex  >/dev/null
$ADB push "$ROOT/drivers/drv_${KV}.sh" $D/drv.sh >/dev/null
for f in g.png g.txt ui.png ui.txt ctype.txt level.txt name.txt price.txt skip.txt; do :; done
$ADB push "$ROOT/assets/g.png"     $D/glyph.png     >/dev/null
$ADB push "$ROOT/assets/g.txt"     $D/glyph.txt     >/dev/null
$ADB push "$ROOT/assets/ui.png"    $D/ov_ui.png     >/dev/null
$ADB push "$ROOT/assets/ui.txt"    $D/ov_ui.txt     >/dev/null
$ADB push "$ROOT/assets/ctype.txt" $D/ov_ctype.txt  >/dev/null
$ADB push "$ROOT/assets/level.txt" $D/ov_level.txt  >/dev/null
$ADB push "$ROOT/assets/name.txt"  $D/ov_name.txt   >/dev/null
$ADB push "$ROOT/assets/price.txt" $D/ov_price.txt  >/dev/null
$ADB push "$ROOT/assets/skip.txt"  $D/ov_skip.txt   >/dev/null
$ADB push "$ROOT/scripts/start_col.sh"  $D/start_col.sh  >/dev/null
$ADB push "$ROOT/scripts/deploy_ov.sh"  $D/deploy_ov.sh  >/dev/null

echo "[2/4] 刷驱动 ..."
$ADB shell "su -c 'cd $D && sh drv.sh 2>&1 | tail -3'"

echo "[3/4] 装 dex + 启动 ..."
$ADB shell "su -c 'mkdir -p $C; chmod 755 $D/start_col.sh $D/deploy_ov.sh'
  sed -i \"s/\r\$//\" $D/start_col.sh $D/deploy_ov.sh
  echo 1 > $D/ov_dec.txt; echo 0 > $D/ov_dec2.txt; echo 0 > $D/ov_dec3.txt; echo 1 > $D/ov_dec4.txt
  chmod 666 $D/ov_dec*.txt
  sh $D/start_col.sh; sh $D/deploy_ov.sh"

echo "[4/4] 状态 ..."
$ADB shell "su -c 'grep -E \"^enc3=|^undrawn=|^drawn=|^pdt=\" $D/esp.txt; grep \"frame \" $C/o.log | tail -1'"
