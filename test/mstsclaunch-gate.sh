#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# system32's mstsc.exe (7825): Remote Desktop Manager's "Open (external)" looks for
# C:\windows\system32\mstsc.exe and watches the process for the session's end. The launcher starts
# what App Paths registers for mstsc.exe (sg-shell's Remote Desktop Connection) with the same
# arguments, waits for it and returns its exit code, as mstsc lasts as long as its session.
#   - mstsc.exe exists in system32;
#   - started with an .rdp file and /f, the registered program gets them, and mstsc.exe returns
#     only when it ends (3 s), with its exit code (7);
#   - charmap.exe, the same launcher, still hands off and returns at once.
#   WINE=/opt/wine-sg/bin/wine test/mstsclaunch-gate.sh   (mutant SG_MUTANT_MSTSC_NO_WAIT, programs/calc/handoff.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-mstsclaunch.XXXXXX)
export XDG_RUNTIME_DIR="$T/xdg" WAYLAND_DISPLAY=sg-no-wayland
mkdir -m 700 "$XDG_RUNTIME_DIR"
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T" 2>/dev/null; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/target.exe" "$HERE/mstsclaunch-target.c" || { echo "FAIL  target did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
S="$WINEPREFIX/drive_c/windows/system32"
[ -f "$S/mstsc.exe" ] && pass "C:\\windows\\system32\\mstsc.exe exists" || fail "no mstsc.exe in system32"
mkdir -p "$WINEPREFIX/drive_c/sg"; cp "$T/target.exe" "$WINEPREFIX/drive_c/sg/rdc.exe"
for n in mstsc.exe charmap.exe; do
    "$WINE" reg add "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\$n" /ve /d 'C:\sg\rdc.exe' /f >/dev/null 2>&1
done
"$WINESERVER" -w
export MSTSCLAUNCH_OUT='C:\sg\args.txt'
start=$(date +%s%N)
"$WINE" 'C:\windows\system32\mstsc.exe' 'C:\users\x\AppData\Local\Temp\RDM\127.0.0.1.rdp' /f >/dev/null 2>&1; code=$?
took=$(( ($(date +%s%N) - start) / 1000000 ))
args=$(tr -d '\r' < "$WINEPREFIX/drive_c/sg/args.txt" 2>/dev/null)
case "$args" in *'"C:\users\x\AppData\Local\Temp\RDM\127.0.0.1.rdp" /f'*|*'C:\users\x\AppData\Local\Temp\RDM\127.0.0.1.rdp /f'*)
    pass "Remote Desktop Connection got the .rdp file and /f" ;;
    *) fail "the program's command line: '$args'" ;; esac
[ "$took" -ge 2900 ] && pass "mstsc.exe returned when the program ended (${took} ms)" || fail "mstsc.exe returned after ${took} ms, before the program (3 s)"
[ "$code" = 7 ] && pass "mstsc.exe returned the program's exit code (7)" || fail "mstsc.exe exit code $code, wanted 7"
rm -f "$WINEPREFIX/drive_c/sg/args.txt"
start=$(date +%s%N)
"$WINE" 'C:\windows\system32\charmap.exe' >/dev/null 2>&1; code=$?
took=$(( ($(date +%s%N) - start) / 1000000 ))
[ "$took" -lt 2900 ] && [ "$code" = 0 ] && pass "charmap.exe still hands off and returns at once (${took} ms)" || fail "charmap.exe: ${took} ms, exit $code"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
