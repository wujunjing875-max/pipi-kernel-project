#!/system/bin/sh
D=/data/local/tmp
C=$D/.c
pkill -9 -f Ov50 2>/dev/null
sleep 2
rm -f $C/oz.dex
cp $D/ov_new.dex $C/oz.dex
mv -f $C/oz.dex $C/o.dex
chmod 644 $C/o.dex
echo "dex_md5=$(md5sum $C/o.dex | cut -d' ' -f1)  size=$(stat -c %s $C/o.dex)"
cd $D
setsid sh -c "CLASSPATH=$C/o.dex exec app_process $D Ov50 2414 1080 0 0 > $C/o.log 2>&1" < /dev/null &
sleep 4
echo "ov_pid=$(ps -A -o PID,NAME | grep app_process | head -1 | tr -s ' ' | cut -d' ' -f2)"
tail -2 $C/o.log
