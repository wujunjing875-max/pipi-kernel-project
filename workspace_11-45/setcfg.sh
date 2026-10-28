#!/bin/sh
# usage: setcfg.sh key=value [key=value ...]
# 修改设备上的 ov_cfg.txt 并可选重启覆盖层 (RESTART=1)
D="C:/Users/Administrator/WorkBuddy/2026-10-26-11-45-21"
S=1faa2a85
LOCAL="$D/ov_cfg.txt"
MSYS2_ARG_CONV_EXCL="*" adb -s $S pull /data/local/tmp/.tpen/ov_cfg.txt "$LOCAL" >/dev/null 2>&1
for kv in "$@"; do
  k=${kv%%=*}; v=${kv#*=}
  if grep -q "^$k=" "$LOCAL"; then
    python - "$LOCAL" "$k" "$v" <<'EOF'
import sys
p,k,v=sys.argv[1],sys.argv[2],sys.argv[3]
L=open(p,encoding='utf-8').read().splitlines()
o=[]
for ln in L:
    o.append(f"{k}={v}" if ln.startswith(k+"=") else ln)
open(p,'w',encoding='utf-8',newline='\n').write("\n".join(o)+"\n")
EOF
  else
    printf '%s=%s\n' "$k" "$v" >> "$LOCAL"
  fi
done
MSYS2_ARG_CONV_EXCL="*" adb -s $S push "$LOCAL" /data/local/tmp/wtmp/ov_cfg.txt >/dev/null 2>&1
MSYS2_ARG_CONV_EXCL="*" adb -s $S shell "su -c 'cp -f /data/local/tmp/wtmp/ov_cfg.txt /data/local/tmp/.tpen/ov_cfg.txt; chmod 644 /data/local/tmp/.tpen/ov_cfg.txt'"
grep -E '^(wall|wstyle|wrange|wbudget|wband|map)=' "$LOCAL"
if [ "$RESTART" = "1" ]; then
  MSYS2_ARG_CONV_EXCL="*" adb -s $S shell "su -c 'sh /data/local/tmp/.tpen/run_ov.sh'"
fi
