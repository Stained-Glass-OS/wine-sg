#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Layout-id keyboard layouts (KLID 00010409 US-Dvorak, 00020409 US-International...)
# load as their 0xfXXX HKLs and the key functions follow their XKB variant
# (patches/sg/8212; Windows' own Layout Ids and the Canadian French KLIDs:
# 8217): test/kbdklid-probe.c. Mutant: SG_MUTANT_KBD_NO_KLID
# (winex11.drv/xkbtables.c, win32u/input.c).
#   WINE=/opt/wine-sg/bin/wine test/kbdklid-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${KBDKLID_DPY:-284}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f /usr/share/X11/xkb/symbols/us ] || { echo "SKIP: no XKB data (xkb-data)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-kbdklid.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
for inc in "" "-I$(dirname "$WINE")/include" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O2 -municode -mconsole $inc -o "$T/kbdklid-probe.exe" "$HERE/kbdklid-probe.c" \
        -luser32 2>/dev/null && { built=1; break; }
    TMPDIR=/var/tmp "$MINGW" -O2 $inc -o "$T/kbdklid-probe.exe" "$HERE/kbdklid-probe.c" \
        -luser32 2>/dev/null && { built=1; break; }
done
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

# a US X keyboard map, as Xvfb starts with
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/kbdklid-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1

# (8217) a prefix updated from 8212 loses the wrong 00010c0c key (wine.inf DelReg)
KEY='HKLM\System\CurrentControlSet\Control\Keyboard Layouts\00010c0c'
"$WINE" reg add "$KEY" /v "Layout Id" /d 0e03 /f >/dev/null 2>&1
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1
"$WINESERVER" -w
if "$WINE" reg query "$KEY" >/dev/null 2>&1; then echo "FAIL  the update leaves the wrong 00010c0c key"; rc=1
else echo "ok    the update removes the wrong 00010c0c key"; fi

[ $rc = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $rc
