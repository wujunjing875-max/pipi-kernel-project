#!/system/bin/sh
D=/data/local/tmp/.c
pkill -9 -f enccatch 2>/dev/null
cd $D
setsid sh -c "exec $D/enccatch > $D/catch.log 2>&1" < /dev/null &
sleep 1
echo started
