#!/system/bin/sh
X="$1"
Y="$2"
RX=$((Y * 10))
RY=$((X * 10))
sendevent /dev/input/event5 3 53 "$RX"
sendevent /dev/input/event5 3 54 "$RY"
sendevent /dev/input/event5 3 57 511
sendevent /dev/input/event5 1 330 1
sendevent /dev/input/event5 0 0 0
usleep 70000
sendevent /dev/input/event5 1 330 0
sendevent /dev/input/event5 3 57 -1
sendevent /dev/input/event5 0 0 0