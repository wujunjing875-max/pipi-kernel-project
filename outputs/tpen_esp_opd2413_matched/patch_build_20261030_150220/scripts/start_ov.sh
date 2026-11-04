#!/system/bin/sh
D=/data/local/tmp
C=$D/.c
pkill -9 -f Ov50 2>/dev/null
sleep 1
rm -f $D/ov_kill
cd $D
setsid sh -c "CLASSPATH=$C/o.dex exec app_process $D Ov50 2414 1080 0 0 > $C/o.log 2>&1" < /dev/null &
sleep 4
echo "=== ov ==="
ps -A -o PID,ETIME,NAME | grep app_process
tail -2 $C/o.log
