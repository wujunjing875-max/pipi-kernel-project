#!/usr/bin/env sh
set -eu
TARGET=${1:?usage: ROLLBACK.sh TARGET_DIRECTORY}
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/BASELINE" && pwd)
case "$TARGET" in ""|/|.) echo "refuse unsafe target: $TARGET" >&2; exit 2;; esac
mkdir -p "$TARGET"
rm -rf -- "$TARGET/build" "$TARGET/out" "$TARGET/tools" "$TARGET/__pycache__"
rm -f -- "$TARGET/build_device_package.py"
mkdir -p "$TARGET/src/C" "$TARGET/src/Java"
cp "$BASE/paradise_fast100.c" "$TARGET/src/C/paradise_fast100.c"
cp "$BASE/Ov50.java" "$TARGET/src/Java/Ov50.java"
printf '%s\n' "restored $TARGET"
