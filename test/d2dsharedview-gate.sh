#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D bitmaps shared from other bitmaps can be drawn (patches/sg/1491),
# under Xvfb: test/d2dsharedview-probe.c draws a bitmap, a view of it that
# CreateSharedBitmap made with bitmap properties given, and an alpha-ignoring
# view, onto a WIC bitmap target and reads the pixels back.  Word's ribbon
# draws its font name and size boxes from such views of its atlas; Wine made
# each view a target that cannot be drawn, and the boxes came out black.
#
#  - the views exist and carry the source bitmap's options;
#  - drawing a view draws the source's pixels (also with alpha ignored).
#
#   WINE=/opt/wine-sg/bin/wine test/d2dsharedview-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${D2DSHAREDVIEW_DPY:-297}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d2dsharedview.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/d2dsharedview-probe.exe" "$HERE/d2dsharedview-probe.c" \
    -ld2d1 -lwindowscodecs -lole32 -luuid || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/d2dsharedview-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check target              "a Direct2D WIC bitmap target"
check source              "a bitmap to share"
check shared_view         "CreateSharedBitmap from it, with properties"
check shared_view_ignore  "and with alpha ignored"
check view_options        "the view has the source's options"
check enddraw             "drawing ends without error"
check source_drawn        "the source bitmap is drawn"
check view_drawn          "the view is drawn (was: nothing)"
check view_ignore_drawn   "the alpha-ignoring view is drawn"
check background_kept     "the rest stays as cleared"
check done                       "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
