#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# doskey.exe and doskey macros (patches/sg/9262; test/doskey-probe.c). Wine had no doskey.exe, so Git for
# Windows' Git CMD -- cmd /K "doskey git=^"...\git.exe^" $*" -- opened with "Can't recognize ...", and the
# console host expanded no macro. As Windows': doskey keeps macros in the console's alias table per program
# (/EXENAME, /MACROS[:ALL|:exe], name=, /?), and a macro typed at the start of a line the program reads is
# expanded ($1-$9, $*, $T, $G $L $B $$), the history keeping the line as typed.
# The probe runs in a real console (conhost with a window, on its own Xvfb).
#   WINE=/opt/wine-sg/bin/wine test/doskey-gate.sh
# Mutants: SG_MUTANT_MACRO_NO_EXPAND (programs/conhost/conhost.c), SG_MUTANT_DOSKEY_NO_ALIAS (programs/doskey/main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${DOSKEY_DPY:-350}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-doskey.XXXXXX)
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    sg_kill_prefix "$WINEPREFIX"
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/doskey-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; echo "RESULT: FAIL"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do python3 -c 'import time; time.sleep(0.25)'; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
# a real console: started by cmd through start /wait; the output goes through a file
cd "$T" && timeout -s KILL 120 "$WINE" start /wait cmd /c 'C:\probe.exe > C:\out.raw' >/dev/null 2>&1 </dev/null
"$WINESERVER" -w
cp "$WINEPREFIX/drive_c/out.raw" "$T/out.raw" 2>/dev/null || : > "$T/out.raw"
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
