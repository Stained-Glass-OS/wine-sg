#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Push buttons and message boxes as Windows 10 draws them (patches/sg/0454):
# a message box's text on the window colour and its buttons on a strip of the
# face colour; buttons flat -- the default one's border in the highlight
# colour, no dark bevel, no dotted focus rectangle. They were Windows 95's
# (field report 2: Setup's Next, installers' message boxes looked old). Edit
# boxes' edges and drop-down lists flat too (0455).
#
#   WINE=/opt/wine-sg/bin/wine test/flatbuttons-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-flatbuttons.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/flatbuttons-probe.exe" "$HERE/flatbuttons-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF
#!/bin/sh
"$WINE" "$T/flatbuttons-probe.exe" > "$T/probe.out" 2>/dev/null &
i=0; while ! grep -q highlight "$T/probe.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
import -window root "$T/shot.png"
"$WINESERVER" -k; sleep 1
"$WINE" "$T/flatbuttons-probe.exe" form > "$T/form.out" 2>/dev/null &
i=0; while ! grep -q text "$T/form.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2; import -window root "$T/form.png"
"$WINESERVER" -k
EOF
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
tr -d '\r' < "$T/probe.out" | sed 's/^/      /'
v() { tr -d '\r' < "$T/probe.out" | awk -v k="$1" -v n="$2" '$1 == k { print $(n + 1) }'; }
rgb() { c=$1; printf '%d,%d,%d' $((c & 255)) $(((c >> 8) & 255)) $(((c >> 16) & 255)); }
px() { convert "$T/shot.png" -format "%[fx:int(255*p{$1,$2}.r)],%[fx:int(255*p{$1,$2}.g)],%[fx:int(255*p{$1,$2}.b)]" info: 2>/dev/null; }
[ -n "$(v yes 1)" ] || { fail "no message box"; exit 1; }
win=$(rgb "$(v window 1)"); face=$(rgb "$(v face 1)"); hi=$(rgb "$(v highlight 1)")
bl=$(v box 1); bt=$(v box 2); yl=$(v yes 1); yt=$(v yes 2); yr=$(v yes 3); yb=$(v yes 4)
# the body: just inside the box's left edge, at the height of the text (below the caption)
body=$(px $((bl + 8)) $(( (bt + yt) / 2 + 10 )))
strip=$(px $((bl + 8)) $((yb - 2)))
[ "$body" = "$win" ] && pass "the message is on the window colour ($win)" || fail "body $body, not $win"
[ "$strip" = "$face" ] && pass "the buttons are on a strip of the face colour ($face)" || fail "strip $strip, not $face"
corner=$(px $((yr - 1)) $((yb - 1)))
[ "$corner" = "$hi" ] && pass "the default button: flat, its border in the highlight colour" || fail "default button's corner $corner, not $hi (a bevel?)"
inner=$(px $((yr - 3)) $((yb - 3)))
[ "$inner" != "0,0,0" ] && [ "$inner" != "105,105,105" ] && pass "and no dark bevel inside it" || fail "bevel at $inner"
# 0455: an edit box's edge and a drop-down list, flat
tr -d '\r' < "$T/form.out" | sed 's/^/      /'
f() { tr -d '\r' < "$T/form.out" | awk -v k="$1" -v n="$2" '$1 == k { print $(n + 1) }'; }
fpx() { convert "$T/form.png" -format "%[fx:int(255*p{$1,$2}.r)],%[fx:int(255*p{$1,$2}.g)],%[fx:int(255*p{$1,$2}.b)]" info: 2>/dev/null; }
if [ -n "$(f edit 1)" ]; then
    fwin=$(rgb "$(f window 1)"); shadow=$(rgb "$(f shadow 1)")
    el=$(f edit 1); et=$(f edit 2); eb=$(f edit 4)
    ey=$(( (et + eb) / 2 ))
    { [ "$(fpx "$el" "$ey")" = "$shadow" ] && [ "$(fpx $((el + 1)) "$ey")" = "$fwin" ]; } \
        && pass "an edit box's edge: one line of the shadow colour, one of the window's" || fail "edit edge $(fpx "$el" "$ey") / $(fpx $((el + 1)) "$ey")"
    cr=$(f combo 3); ct=$(f combo 2); cbm=$(f combo 4)
    [ "$(fpx $((cr - 4)) $((ct + 4)))" = "$fwin" ] && pass "a drop-down list's button: flat, on the window colour" \
        || fail "combo button corner $(fpx $((cr - 4)) $((ct + 4))), not $fwin (a raised button?)"
    [ "$(fpx "$cr" $(( (ct + cbm) / 2 )))" != "0,0,0" ] || fail "a black edge on the drop-down list"
else fail "no form"; fi

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
