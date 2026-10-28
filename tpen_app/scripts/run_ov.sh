#!/system/bin/sh
D=/data/local/tmp/.tpen
pkill -9 -f Ov50 2>/dev/null
sleep 1
cd $D
setsid sh -c "CLASSPATH=$D/ov.dex exec app_process $D Ov50 3392 2400 0 0 > $D/o.log 2>&1" </dev/null >/dev/null 2>&1 &
sleep 4
echo ov_started
