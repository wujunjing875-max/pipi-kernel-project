#!/system/bin/sh
# 一键启动：驱动 -> overlay(Ov45) -> 采集端(p)
D=/data/local/tmp
C=$D/.c

# 1) 让旧 overlay 自杀
touch $D/ov_kill 2>/dev/null
sleep 1
rm -f $D/ov_kill

# 2) 杀掉旧采集端（cmdline 被抹成 kworker，只能靠 exe 链接找）
for d in /proc/[0-9]*; do
  e=$(readlink $d/exe 2>/dev/null)
  case "$e" in
    *"/.c/p"*) kill -9 ${d#/proc/} 2>/dev/null ;;
  esac
done

# 3) 驱动（已加载会失败，忽略）
sh $C/drv.sh > $C/drv.log 2>&1

# 4) overlay
cd $D
setsid sh -c "CLASSPATH=$C/o.dex exec app_process $D Ov50 2414 1080 0 0 > $C/o.log 2>&1" < /dev/null &
sleep 4

# 5) 采集端
setsid sh -c "exec $C/p > $C/p.log 2>&1" < /dev/null &
sleep 3

echo "=== drv ==="; tail -2 $C/drv.log
echo "=== esp ==="; head -4 $D/esp.txt 2>/dev/null
