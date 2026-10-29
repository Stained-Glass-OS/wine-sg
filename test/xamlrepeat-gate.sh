#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml RepeatButton and Thumb (patches/sg/6187): a RepeatButton clicks when pressed, after
# Delay and every Interval while held, not again when released; a Thumb raises DragStarted (where it
# was taken), DragDelta (each move, also off it) and DragCompleted (the whole drag), IsDragging
# meanwhile -- the parts of a templated ScrollBar (Windows Terminal's); a templated button draws only
# its template, no face of ours under it. test/xamlrepeat-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlrepeat-gate.sh
# Mutants (dlls/windows.ui.xaml/buttons.c): SG_MUTANT_REPEAT_BUTTON_ONCE, SG_MUTANT_REPEAT_RELEASE_CLICK,
# SG_MUTANT_THUMB_NO_DELTA, SG_MUTANT_TEMPLATED_BUTTON_FACE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlrepeat.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlrepeat-probe.exe" "$HERE/xamlrepeat-probe.c" -lruntimeobject -luser32 -lgdi32 -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlrepeat-probe.exe" "$WINEPREFIX/drive_c/"
python3 "$HERE/mkxbf.py" "$WINEPREFIX/drive_c" || { echo "FAIL  fixtures not written"; exit 1; }
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlrepeat-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^defaults=00000000,250,250$' "RepeatButton activates; Delay and Interval 250 ms"
chk '^load=00000000$' "markup with RepeatButton and Thumb"
chk '^markup=150,50$' "Delay and Interval from markup"
chk '^templated=1,1$' "a templated button draws only its template; an untemplated one its face"
chk '^pressed=1$' "a click when pressed"
chk '^held=1 released=0 stopped=1$' "clicks while held; none when released, none after"
chk '^started=1,10,10$' "DragStarted where the thumb was taken"
chk '^delta=2,30,25,1$' "DragDelta for each move, IsDragging"
chk '^completed=1,30,25,0,0$' "DragCompleted with the whole drag; IsDragging after"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
