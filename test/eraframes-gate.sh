#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The window frames of the Horizon and Glass looks (patches/sg/0742). David:
# make each look complete, "very close to each era's look" -- window frames
# and title bars too, not only the taskbar and Start. HKCU\Software\Stained
# Glass\Style "Frame" 1 (Horizon) draws a blue gradient title bar across the
# whole window, a blue frame, rounded caption buttons with a red Close and
# the top corners round; 2 (Glass) a pale blue-grey one; 0 keeps the flat
# frame. A maximized window keeps square corners. The checks read the
# screen, the desktop a flat green.
#
#   WINE=/opt/wine-sg/bin/wine test/eraframes-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
unset DISPLAY WAYLAND_DISPLAY
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run import convert; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-eraframes.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/rounded-probe.exe" "$HERE/rounded-probe.c" -ldwmapi -lgdi32 -lcomctl32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
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
frame() { "$WINE" reg add 'HKCU\\Software\\Stained Glass\\Style' /v Frame /t REG_DWORD /d \$1 /f >/dev/null 2>&1; sleep 1; }
shot() { sleep 4; import -window root "$T/\$1.png"; }
frame 1
"$WINE" rounded-probe.exe win Horizon 100 100 400 250 & shot horizon
"$WINE" taskkill /f /im rounded-probe.exe >/dev/null 2>&1; sleep 1
frame 2
"$WINE" rounded-probe.exe win Glass 100 100 400 250 & shot glass
"$WINE" taskkill /f /im rounded-probe.exe >/dev/null 2>&1; sleep 1
frame 0
"$WINE" rounded-probe.exe win Flat 100 100 400 250 & shot flat
"$WINE" taskkill /f /im rounded-probe.exe >/dev/null 2>&1; sleep 1
frame 1
"$WINE" rounded-probe.exe win Max 0 0 300 200 max & shot horizonmax
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 1024x700x24" "$T/session.sh" > "$T/session.out" 2>&1

px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
is() { px "$1" "$2" "$3" | awk -F, "{ r = \$1; g = \$2; b = \$3; exit !($4) }"; }
green() { [ "$(px "$@")" = "0,255,0" ]; }
for s in horizon horizonmax glass flat; do [ -f "$T/$s.png" ] || { fail "no screenshot $s: $(tail -3 "$T/session.out")"; echo "RESULT: FAIL"; exit 1; }; done

# the title bar: the window's top 20 px, sampled in the middle (left of the buttons)
is horizon 250 112 'b > r + 100 && b > 180' && is horizon 250 103 'b > r + 100' \
    && pass "Horizon: a blue title bar ($(px horizon 250 112))" || fail "Horizon title bar: $(px horizon 250 112) $(px horizon 250 103)"
[ "$(px horizon 250 103)" != "$(px horizon 250 118)" ] && pass "and a gradient, lit at the top ($(px horizon 250 103) -> $(px horizon 250 118))" \
    || fail "Horizon title bar flat: $(px horizon 250 103) $(px horizon 250 118)"
is horizon 101 300 'b > r + 80' && is horizon 498 300 'b > r + 80' \
    && pass "a blue frame down the sides ($(px horizon 101 300))" || fail "Horizon frame: $(px horizon 101 300) $(px horizon 498 300)"
green horizon 100 100 && green horizon 499 100 && ! green horizon 100 349 \
    && pass "the top corners round, the bottom ones square" || fail "Horizon corners: $(px horizon 100 100) $(px horizon 499 100) bottom $(px horizon 100 349)"
rc=0; for x in $(seq 470 2 496); do is horizon "$x" 112 'r > b + 60 && r > 150' && rc=1; done
[ $rc = 1 ] && pass "a red Close button at the right" || fail "no red Close button: $(px horizon 480 112) $(px horizon 488 112)"
! green horizonmax 1 1 && pass "maximized: square corners" || fail "maximized corner: $(px horizonmax 1 1)"
is glass 250 112 'b > r + 25 && r > 140 && b > 200' \
    && pass "Glass: a pale blue-grey title bar ($(px glass 250 112))" || fail "Glass title bar: $(px glass 250 112)"
green glass 100 100 && pass "its top corners round too" || fail "Glass corner: $(px glass 100 100)"
rc=0; for x in $(seq 470 2 496); do is glass "$x" 112 'r > b + 60' && rc=1; done
[ $rc = 1 ] && pass "and a red Close button" || fail "Glass Close: $(px glass 480 112)"
! is flat 250 112 'b > r + 25' && ! green flat 100 100 \
    && pass "Frame 0: the flat frame, unchanged ($(px flat 250 112), square corners)" || fail "flat: $(px flat 250 112) corner $(px flat 100 100)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
