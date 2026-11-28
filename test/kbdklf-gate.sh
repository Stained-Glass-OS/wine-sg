#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ActivateKeyboardLayout's flags (patches/sg/9163; ledger i0535): KLF_REORDER puts the layout first in the
# loaded list (GetKeyboardLayoutList, HKL_NEXT go by it); KLF_RESET keeps how Caps Lock is turned off in the
# user's profile, HKCU\Keyboard Layout "Attributes" (KLF_SHIFTLOCK: either Shift key); no FIXME for them.
# Wine ignored them all with "flags ... not supported".
#   WINE=/opt/wine-sg/bin/wine test/kbdklf-gate.sh
# Mutant: SG_MUTANT_KBD_KLF_FIXME (win32u/input.c) fails all.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDKLF_DPY:-482}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdklf.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$T/prefix"; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/kbdklf-probe.c" -luser32 -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:-fixme+keyboard} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
WINEDEBUG=-all timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 60 "$WINE" "$T/probe.exe" > "$T/out" 2> "$T/err" </dev/null
tr -d '\r' < "$T/out" | sed 's/^/      /'
o=$(tr -d '\r' < "$T/out" | tr '\n' '|')
rc=0
has() { case "$o" in *"$2"*) echo "PASS  $1" ;; *) echo "FAIL  $1: want '$2'"; rc=1 ;; esac; }
has "German and French loaded" "LOADED 04070407 040c040c|"
has "KLF_REORDER: French first, the others after in their order" "REORDER 040c040c 04090409 04070407|"
has "KLF_RESET | KLF_SHIFTLOCK: Attributes 0x10000 (either Shift turns Caps Lock off)" "ATTR 10000|"
has "KLF_RESET alone: Attributes 0 (Caps Lock again)" "ATTR 10000|ATTR 0|"
if grep -q 'not supported' "$T/err"; then echo "FAIL  a FIXME for the flags: $(grep -m1 'not supported' "$T/err")"; rc=1
else echo "PASS  no FIXME for the flags"; fi
[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
