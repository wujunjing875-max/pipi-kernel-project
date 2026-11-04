#!/system/bin/sh
# 通用采集端部署：/data/local/tmp/p_latest -> .c/p
D=/data/local/tmp/.c
for d in /proc/[0-9]*; do
  e=$(readlink $d/exe 2>/dev/null)
  case "$e" in */.c/p*) kill -9 ${d#/proc/} 2>/dev/null;; esac
done
sleep 2
rm -f $D/pt
cp /data/local/tmp/p_latest $D/pt
mv -f $D/pt $D/p
chmod 755 $D/p
echo "p_md5=$(md5sum $D/p | cut -d' ' -f1)  size=$(stat -c %s $D/p)"
setsid sh -c "exec $D/p > $D/p.log 2>&1" < /dev/null &
sleep 2
echo "started"
