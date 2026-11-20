#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# user32/win32u/server: a local window class registered with an instance below 0x10000 that is not a
# module is found with another such value, as on Windows (patches/sg/8409). NAPS2's TWAIN worker
# registers its message pump's class and creates the window each with Process.GetCurrentProcess().Handle,
# a new process handle per call: Wine matched instances below 0x10000 exactly (Win16 selectors), the
# window was never created, the pump's posts went to the wrong thread and the TWAIN device search
# never ended. test/classinst-probe.c, 64-bit and 32-bit (wow64), on a private Xvfb:
#   - the class is found with the value it was registered with (as before);
#   - GetClassInfo and CreateWindowEx find it with a second process handle;
#   - a message posted to that window from another thread reaches the window's thread;
#   - instances whose high words differ (0x12340000, 0x56780000) still do not find each other.
#   WINE=/opt/wine-sg/bin/wine test/classinst-gate.sh
# Mutant (win32u + server): SG_MUTANT_CLASS_SMALL_INSTANCE_EXACT (dlls/win32u/class.c, server/class.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-classinst.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -o "$T/classinst64.exe" "$HERE/classinst-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O2 -o "$T/classinst32.exe" "$HERE/classinst-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
unset WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w

fail=0
for b in 64 32; do
    timeout -s KILL 60 "$WINE" "$T/classinst$b.exe" >"$T/out$b.raw" 2>/dev/null </dev/null
    tr -d '\r' < "$T/out$b.raw" > "$T/out$b"
    grep -E '^(bits|window):' "$T/out$b" | sed "s/^/INFO  $b-bit /"
    check() {
        if grep -qx "$1=1" "$T/out$b"; then echo "PASS  $b-bit: $2"; else echo "FAIL  $b-bit: $2"; fail=1; fi
    }
    check handles "two different process handles, both below 0x10000"
    check register "the class registers with the first"
    check same "it is found with the value it was registered with"
    check classinfo "GetClassInfo finds it with the second process handle"
    check window "CreateWindowEx finds it with the second process handle"
    check posted "a message posted from another thread reaches the window"
    check bigapart "instances whose high words differ still do not find each other"
    check done "the probe ran to the end"
done
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
