#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A frame extended into the client area (6085). Paint.NET 5 and SumatraPDF draw their own title bar: the client
# area covers the caption (WM_NCCALCSIZE), DwmExtendFrameIntoClientArea extends the frame over the top band, the
# program paints that band transparent black under its tabs and title, and DWM shows the frame there and draws
# and hit-tests the caption buttons (DwmDefWindowProc). Wine had no DWM: a black bar with no minimise, maximise
# or close button. Now the display driver gives the band's transparent pixels the caption colour and draws the
# buttons where DWMWA_CAPTION_BUTTON_BOUNDS says they are (opaque black a program draws there -- GDI+, Direct2D,
# alpha 255 -- stays black; only transparent black shows the frame), and DwmDefWindowProc answers WM_NCHITTEST for them,
# so pressing one does what a caption button does.
# The probe is such a window (black client, a blue square in the 32 px band) on a shell desktop.
#   WINE=/opt/wine-sg/bin/wine test/dwmframe-gate.sh   (SG_XDISPLAY=N to use Xvfb :N)
# Mutants: SG_MUTANT_DWM_FRAME_BLACK, SG_MUTANT_DWM_NO_BUTTONS (winex11), SG_MUTANT_DWM_NO_HITTEST (dwmapi).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb xdotool import convert $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-dwmframe.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
XP=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${ARTIFACTS:-}" ] && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -mwindows -o "$T/probe.exe" "$HERE/dwmframe-probe.c" -ldwmapi 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
if [ -n "${SG_XDISPLAY:-}" ]; then
    Xvfb ":$SG_XDISPLAY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    echo "$SG_XDISPLAY" > "$T/display"; sleep 1
else
    Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" explorer /desktop=shell,1024x700 'C:\probe.exe' 'C:\probe.out' >/dev/null 2>&1 &
i=0; while ! grep -q ready=1 "$WINEPREFIX/drive_c/probe.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 3
out=$(tr -d '\r' < "$WINEPREFIX/drive_c/probe.out")
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
import -window root "$T/shot.png"
wx=$(v window | cut -d, -f1); wy=$(v window | cut -d, -f2)
b=$(v buttons); bl=$(echo "$b" | cut -d, -f1); bt=$(echo "$b" | cut -d, -f2); br=$(echo "$b" | cut -d, -f3); bb=$(echo "$b" | cut -d, -f4)
px() { convert "$T/shot.png" -format "%[fx:int(255*p{$1,$2}.r)],%[fx:int(255*p{$1,$2}.g)],%[fx:int(255*p{$1,$2}.b)]" info: 2>/dev/null | tail -n 1; }
dark() {   # dark pixels in a rectangle of the screen
    convert "$T/shot.png" -crop "$3x$4+$1+$2" +repage -fx '(r+g+b<0.6) ? 1 : 0' -format '%[fx:int(mean*w*h+0.5)]' info: 2>/dev/null | tail -n 1
}
[ "$(v extend)" = 0 ] && pass "DwmExtendFrameIntoClientArea(top 32) succeeds" || fail "extend: $(v extend)"
[ -n "$wx" ] && [ -n "$bl" ] || { fail "the probe did not report its window"; echo "RESULT: FAIL"; exit 1; }
p=$(px $((wx + 150)) $((wy + 16)))
[ "$p" = "255,255,255" ] && pass "the band's transparent pixels show the frame (white), not black ($p)" || fail "band pixel: $p"
p=$(px $((wx + 50)) $((wy + 16)))
[ "$p" = "0,0,255" ] && pass "what the program drew there stays ($p)" || fail "blue square: $p"
p=$(px $((wx + 90)) $((wy + 16)))
[ "$p" = "0,0,0" ] && pass "opaque black there (alpha 255, as GDI+ draws it) stays black ($p)" || fail "opaque black square: $p"
p=$(px $((wx + 150)) $((wy + 120)))
[ "$p" = "0,0,0" ] && pass "below the band its black stays black ($p)" || fail "client below the band: $p"
w=$(( (br - bl) / 3 ))
n=0
for k in 0 1 2; do
    d=$(dark $((wx + bl + k * w)) $((wy + bt)) $w $((bb - bt)))
    echo "      button $k: $d dark pixels"
    [ "${d:-0}" -ge 6 ] && [ "${d:-0}" -le 80 ] && n=$((n + 1))
done
[ $n = 3 ] && pass "three caption buttons are drawn (a glyph in each)" || fail "caption buttons drawn: $n of 3"
[ "$(v hit_close)" = 20 ] && [ "$(v hit_max)" = 9 ] && [ "$(v hit_min)" = 8 ] \
    && pass "DwmDefWindowProc hit-tests them: HTCLOSE, HTMAXBUTTON, HTMINBUTTON" \
    || fail "hit tests: close $(v hit_close) max $(v hit_max) min $(v hit_min)"
[ "$(v hit_band)" = 2 ] && [ "$(v hit_client)" = 1 ] && pass "elsewhere the program answers (HTCAPTION in its band, HTCLIENT)" \
    || fail "other hit tests: band $(v hit_band) client $(v hit_client)"
cx=$((wx + br - w / 2)); cy=$((wy + (bt + bb) / 2))
xdotool mousemove "$cx" "$cy"; sleep 0.5; xdotool mousedown 1; sleep 0.3; xdotool mouseup 1
i=0; while ! grep -q closed=1 "$WINEPREFIX/drive_c/probe.out" 2>/dev/null && [ $i -lt 20 ]; do sleep 0.5; i=$((i + 1)); done
grep -q closed=1 "$WINEPREFIX/drive_c/probe.out" && pass "clicking the drawn close button closes the window" || fail "the close button did nothing"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
