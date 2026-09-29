#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Rounded style rounds windows' corners (patches/sg/0483, 0492). David
# asked for a look like newer Windows beside the Windows 10 one. With
# HKCU\Software\Stained Glass\Style "Rounded" = 1, win32u gives each
# top-level window with a title bar 8 px rounded corners -- a window region
# the server clips by, so what is below paints the corners, but one
# GetWindowRgn does not show. Popup menus are rounded too, tooltips by 4 px.
# Square corners stay: with the style off, maximized, with the program's own
# region, and when the program asks (DWMWA_WINDOW_CORNER_PREFERENCE
# DONOTROUND; ROUNDSMALL is 4 px). A resize shapes the window again. The
# checks read the screen: a window's corner pixel is the green desktop
# behind it when rounded; a menu's, the window below it.
#
#   WINE=/opt/wine-sg/bin/wine test/rounded-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run import convert; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-rounded.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/rounded-probe.exe" "$HERE/rounded-probe.c" -ldwmapi -lgdi32 -lcomctl32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/rounded-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '0 255 0' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
P() { "$WINE" rounded-probe.exe "\$@" 2>/dev/null | tr -d '\r'; }
style() { "$WINE" reg add 'HKCU\\Software\\Stained Glass\\Style' /v Rounded /t REG_DWORD /d \$1 /f >/dev/null 2>&1; sleep 1; }
shot() { sleep 3; import -window root "$T/\$1.png"; }
"$WINE" rounded-probe.exe win Off 40 40 300 200 & sleep 4
style 1
"$WINE" rounded-probe.exe win On 380 40 300 200 &
"$WINE" rounded-probe.exe win Donot 40 300 300 200 donot &
"$WINE" rounded-probe.exe win Rgn 380 300 300 200 rgn &
"$WINE" rounded-probe.exe win Small 720 300 280 200 small &
shot one
xwininfo -root -tree > "$T/tree.out" 2>&1
for w in \$(awk '/"On"|"Small"/ {print \$1}' "$T/tree.out"); do xwininfo -shape -id \$w; done > "$T/shape.out" 2>&1
P pref Donot > "$T/pref.out"; P pref On >> "$T/pref.out"
P rgn On > "$T/rgn.out"; P rgn Rgn >> "$T/rgn.out"
P resize On 320 240; shot resized
style 0
P resize On 300 200; shot off
style 1
"$WINE" rounded-probe.exe win Max 0 0 300 200 max & shot max
"$WINE" rounded-probe.exe menu 600 100 & "$WINE" rounded-probe.exe tip 600 450 & shot popups
"$WINE" rounded-probe.exe win Fly 100 420 200 150 popupround & "$WINE" rounded-probe.exe win Plain 350 420 200 150 popup & shot flyouts
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 1024x700x24" "$T/session.sh" > "$T/session.out" 2>&1

px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
green() { [ "$(px "$@")" = "0,255,0" ]; }
[ -f "$T/one.png" ] || { fail "no screenshot: $(tail -3 "$T/session.out")"; echo "RESULT: FAIL"; exit 1; }

green one 41 41 && fail "a window shown before the style was on has rounded corners" || pass "style off: square corners ($(px one 41 41))"
green one 381 41 && green one 679 41 && green one 381 239 && green one 679 239 \
    && pass "style on: all four corners of a new window are rounded" || fail "rounded corners: $(px one 381 41) $(px one 679 41) $(px one 381 239) $(px one 679 239)"
green one 530 40 || green one 380 140 || green one 384 44 \
    && fail "the rounding takes more than the corners: $(px one 530 40) $(px one 380 140) $(px one 384 44)" || pass "and only the corners: the edges and the inside are the window"
green one 41 301 && fail "DWMWCP_DONOTROUND is rounded" || pass "DWMWA_WINDOW_CORNER_PREFERENCE DONOTROUND keeps square corners"
green one 381 301 && fail "a window with its own region was rounded" || pass "a program's own window region is its shape"
green one 720 300 && ! green one 722 302 && pass "ROUNDSMALL: a smaller radius" || fail "ROUNDSMALL: $(px one 720 300) $(px one 722 302)"
[ "$(sed -n 1p "$T/rgn.out")" = "rgn=0" ] && [ "$(sed -n 2p "$T/rgn.out")" = "rgn=2" ] \
    && pass "GetWindowRgn: the rounded window has no region (ERROR), the program's region is its own" || fail "GetWindowRgn: $(cat "$T/rgn.out" | tr '\n' ' ')"
grep -q "pref=1 hr=0" "$T/pref.out" && grep -q "pref=0 hr=0" "$T/pref.out" && pass "DwmGetWindowAttribute reads the preference back" || fail "pref: $(cat "$T/pref.out")"
green resized 381 41 && green resized 699 279 && ! green resized 679 239 \
    && pass "a resized window is shaped again at its new size" || fail "resized: $(px resized 381 41) $(px resized 699 279) $(px resized 679 239)"
green off 381 41 && fail "still rounded after the style was turned off" || pass "the style turned off: square again at the next change"
[ -f "$T/max.png" ] && ! green max 1 1 && pass "a maximized window keeps square corners" || fail "maximized: $(px max 1 1)"
# a window without a title bar that asks for round corners (a flyout, 0497):
# over the maximized window, which shows through its corners; one that does
# not ask stays square
[ "$(px flyouts 100 420)" = "$(px flyouts 96 416)" ] && [ "$(px flyouts 350 420)" != "$(px flyouts 346 416)" ] \
    && pass "a flyout that asks (DWMWCP_ROUND) is rounded; a plain popup is not" || fail "flyout: $(px flyouts 100 420) below $(px flyouts 96 416); plain $(px flyouts 350 420)"
# over the maximized window: its own colour shows through the corners (it
# paints there), not the menu's edge -- nor black, which a shape the server
# did not know of left
[ "$(px popups 601 101)" = "$(px popups 596 96)" ] && [ "$(px popups 610 106)" != "$(px popups 596 96)" ] \
    && pass "a popup menu has rounded corners, the window below showing through" || fail "menu corner: $(px popups 601 101) (below: $(px popups 596 96)) inside $(px popups 610 106)"
[ "$(px popups 600 450)" = "$(px popups 596 446)" ] && [ "$(px popups 604 454)" != "$(px popups 596 446)" ] \
    && pass "and a tooltip, a smaller radius" || fail "tooltip corner: $(px popups 600 450) (below: $(px popups 596 446)) inside $(px popups 604 454)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
