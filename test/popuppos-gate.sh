#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CalculatePopupWindowPosition (patches/sg/0511). Word delay-loads it for its
# sign-in flyout; the stub ended Word ("Sign in or create account" gave an
# unhandled 0xc06d007f). A popup is aligned as the TPM_ flags ask, flips to
# the anchor's other side at the monitor's edge, keeps out of the exclude
# rectangle along its primary axis (below/above a button with TPM_VERTICAL,
# beside a menu item otherwise), and is pushed inside the monitor.
#
#   WINE=/opt/wine-sg/bin/wine test/popuppos-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-popuppos.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/popuppos-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
# the fields: ok left top (right gap) (bottom gap), relative to the monitor
lt() { v "$1" | cut -d' ' -f1-3; }
[ "$(v export)" = 1 ] && pass "user32 exports CalculatePopupWindowPosition" || fail "export: $(v export)"
[ "$(lt plain)" = "1 100 100" ] && pass "left/top aligned: the anchor is the popup's top left" || fail "plain: $(v plain)"
[ "$(lt rightbottom)" = "1 50 60" ] && pass "right/bottom aligned: the anchor is its bottom right" || fail "rightbottom: $(v rightbottom)"
[ "$(lt center)" = "1 175 180" ] && pass "centred on the anchor" || fail "center: $(v center)"
[ "$(v flipright | cut -d' ' -f4)" = 10 ] && pass "at the monitor's right edge it opens left of the anchor" || fail "flipright: $(v flipright)"
[ "$(v flipbottom | cut -d' ' -f5)" = 10 ] && pass "at the bottom edge it opens above the anchor" || fail "flipbottom: $(v flipbottom)"
[ "$(v clamp | cut -d' ' -f2)" = 0 ] && pass "wider than the monitor: it starts at the monitor's left" || fail "clamp: $(v clamp)"
[ "$(lt below)" = "1 100 120" ] && pass "TPM_VERTICAL: below the button it excludes" || fail "below: $(v below)"
[ "$(v above | cut -d' ' -f5)" = 30 ] && pass "TPM_VERTICAL: above the button when there is no room below" || fail "above: $(v above)"
[ "$(lt beside)" = "1 180 100" ] && pass "horizontal: beside the excluded item" || fail "beside: $(v beside)"
[ "$(v leftside | cut -d' ' -f4)" = 90 ] && pass "horizontal: left of the item at the right edge" || fail "leftside: $(v leftside)"
[ "$(lt clear)" = "1 100 100" ] && pass "an exclude rectangle it does not touch changes nothing" || fail "clear: $(v clear)"
[ "$(lt rtl)" = "1 50 100" ] && pass "TPM_LAYOUTRTL mirrors the horizontal alignment" || fail "rtl: $(v rtl)"
[ "$(v nullanchor)" = "0 87" ] && pass "no anchor: FALSE, ERROR_INVALID_PARAMETER" || fail "nullanchor: $(v nullanchor)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
