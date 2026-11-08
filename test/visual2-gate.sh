#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Composition IVisual2 (patches/sg/7707): RelativeOffsetAdjustment and RelativeSizeAdjustment,
# parts of a visual's offset and size relative to its parent's size (the root visual's parent is its desktop
# window target's window), applied where the compositor draws; ParentForTransform kept. Teams' title bar
# overlay asks for IVisual2 (it crashed on E_NOINTERFACE). The probe draws into its own window and reads the
# pixels: a sprite with no size of its own fills the right half, a corner sprite sits half the height down,
# and both follow the window's size at the next commit (a resize alone does not commit: dcomp's own behaviour). test/visual2-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/visual2-gate.sh
# Mutants: SG_MUTANT_NO_VISUAL2, SG_MUTANT_VISUAL2_NOT_APPLIED (dlls/dcomp/composition.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-visual2.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/visual2-probe.exe" "$HERE/visual2-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid -ld2d1 \
    || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DPY="${VISUAL2_DPY:-278}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do python3 -c "import time; time.sleep(0.25)"; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/visual2-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" visual2-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^visual2=1$' "a visual is IVisual2"
chk '^defaults=1$' "the adjustments start at zero, no parent for transform"
chk '^roundtrip=1$' "RelativeSizeAdjustment reads back what was set"
chk '^right_half=1 ' "a sprite sized and moved by halves of its parent (the window-sized root) fills the right half"
chk '^left_half_not=1 ' "... and not the left"
chk '^corner=1 ' "an offset half the parent's height down puts the corner sprite there"
chk '^corner_not_top=1 ' "... and not at the top"
chk '^follows_window=1 ' "when the window has grown, what is relative to it follows at the next commit"
[ "$RC" = 0 ] && echo "visual2 gate: PASS" || echo "visual2 gate: FAIL"
exit "$RC"
