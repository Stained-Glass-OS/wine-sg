#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A UIElement's Opacity is drawn (patches/sg/6216): at 0 it and its content are not drawn, below 1
# through a layer at that opacity (nested ones multiply), and setting it draws again. Opacity was
# ignored: Windows Terminal's settings menu lit every item's selection marker. test/xamlopacity-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlopacity-gate.sh
# Mutants: SG_MUTANT_XAML_OPACITY_IGNORED (element.c), SG_MUTANT_OPACITY_NO_LAYER (render.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlopacity.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlopacity-probe.exe" "$HERE/xamlopacity-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-264}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
cp "$T/xamlopacity-probe.exe" "$C/"
python3 "$HERE/mkpri.py" "$C/resources.pri" || { echo "FAIL  resources.pri not written"; exit 1; }
python3 "$HERE/mkxbf.py" "$C" || { echo "FAIL  fixtures not written"; exit 1; }
mkdir -p "$C/Lib"
printf '%s' '<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"><TextBlock x:Name="T" x:Uid="LibUid" Text="markup"/></Grid>' > "$C/Lib/LibPage.xaml"
out=$(cd "$C" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlopacity-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
rng() {   # rng NAME LOW HIGH WHAT
    v=$(printf '%s\n' "$out" | tr ' ' '\n' | sed -n "s/^$1=\(-\{0,1\}[0-9]*\)\$/\1/p" | head -n 1)
    if [ -n "$v" ] && [ "$v" -ge "$2" ] && [ "$v" -le "$3" ]; then printf 'PASS  %s (%s)\n' "$4" "$v"; else printf 'FAIL  %s (%s)\n' "$4" "${v:-none}"; RC=1; fi
}
rng hidden 0 3 "Opacity 0: not drawn"
rng half 42 58 "Opacity 0.5: half"
rng quarter 18 32 "0.5 inside 0.5: a quarter"
chk ' full=9[7-9],\(9[7-9]\|100\)$\| full=100,\(9[7-9]\|100\)$' "no Opacity and Opacity 1: solid"
chk '^set=0.0 now=[0-3]$' "set to 0: drawn again, hidden"
chk '^done=1$' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
