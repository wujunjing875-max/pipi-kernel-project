#!/system/bin/sh
X1="$1"; Y1="$2"; X2="$3"; Y2="$4"
rx(){ echo $(( $1 * 10 )); }
# event5 axes are swapped: ABS_MT_POSITION_X=screenY*10, Y=screenX*10
sendevent /dev/input/event5 3 53 $((Y1 * 10))
sendevent /dev/input/event5 3 54 $((X1 * 10))
sendevent /dev/input/event5 3 57 701
sendevent /dev/input/event5 1 330 1
sendevent /dev/input/event5 0 0 0
for N in 1 2 3 4 5; do
 X=$((X1 + (X2-X1)*N/5)); Y=$((Y1 + (Y2-Y1)*N/5))
 sendevent /dev/input/event5 3 53 $((Y * 10))
 sendevent /dev/input/event5 3 54 $((X * 10))
 sendevent /dev/input/event5 0 0 0
 usleep 70000
done
sendevent /dev/input/event5 1 330 0
sendevent /dev/input/event5 3 57 -1
sendevent /dev/input/event5 0 0 0