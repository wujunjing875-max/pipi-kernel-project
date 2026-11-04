#!/system/bin/sh
for d in /proc/[0-9]*; do
 e=$(readlink "$d/exe" 2>/dev/null || true)
 case "$e" in /data/local/tmp/*) echo "${d#/proc/} $e";; esac
done