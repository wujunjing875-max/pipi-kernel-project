#!/system/bin/sh
C="$1"
sendevent /dev/input/event0 1 "$C" 1
sendevent /dev/input/event0 0 0 0
usleep 90000
sendevent /dev/input/event0 1 "$C" 0
sendevent /dev/input/event0 0 0 0