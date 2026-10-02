#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What winex11 tells the desktop's compositor (patches/sg/0744; sg-compositor's
# sg-deskcomp draws the Windows programs' windows with drop shadows and real
# alpha -- David: "The apps need to have drop shadows like in windows. We also
# should support alpha in windows.").
#   - the desktop's picture: _SG_DESKTOP_PIXMAP on the virtual desktop's X
#     window names a pixmap that stays alive, showing the desktop (here a flat
#     green);
#   - _SG_SHADOW on each window: 1 for a window with a title bar, 2 for a
#     popup menu (its class has CS_DROPSHADOW), 0 for the taskbar;
#   - Alt+Tab's switcher asks for a shadow too (0746, CS_DROPSHADOW);
#   - _SG_ACRYLIC (0745): the taskbar frosted, 85% opaque, while
#     Personalization > Colors > Transparency effects is on (the default),
#     and not once it is turned off.
# The third part, no backdrop for alpha popups while the compositor runs, is
# checked by sg-compositor's deskcomp gate (pixels change with what is below).
#
#   WINE=/opt/wine-sg/bin/wine test/deskprops-gate.sh
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
for t in xvfb-run xprop xwininfo cc; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-deskprops.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/rounded-probe.exe" "$HERE/rounded-probe.c" -ldwmapi -lgdi32 -lcomctl32 || { fail "probe did not build"; exit 1; }
cc -O2 -o "$T/pixmap-pixel" "$HERE/pixmap-pixel.c" -lX11 2>/dev/null || { echo "SKIP: cannot build pixmap-pixel (libx11-dev)"; exit 77; }
"$MINGW" -O2 -municode -o "$T/eraschemes-probe.exe" "$HERE/eraschemes-probe.c" -luxtheme -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/rounded-probe.exe" "$T/eraschemes-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '0 255 0' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
"$WINE" rounded-probe.exe win Framed 100 100 300 200 &
sleep 3
"$WINE" rounded-probe.exe menu 500 100 &
sleep 3
D=\$(xwininfo -root -tree | awk '/"shell - Wine Desktop"/ { print \$1; exit }')
xprop -id \$D _SG_DESKTOP_PIXMAP > "$T/pixmap.out" 2>&1
P=\$(sed -n 's/.*# //p' "$T/pixmap.out")
[ -n "\$P" ] && "$T/pixmap-pixel" \$P 400 300 > "$T/pixel.out" 2>&1
for c in \$(xwininfo -id \$D -children | awk '/^ +0x/ { print \$1 }'); do
    printf '%s %s %s\\n' "\$c" "\$(xwininfo -id \$c | awk '/Map State/ { print \$3 }')" "\$(xprop -id \$c _SG_SHADOW WM_NAME 2>&1 | tr '\\n' ' ')"
done > "$T/children.out"
xwininfo -id \$D -children > "$T/tree.out"
TB=\$(xwininfo -id \$D -children | awk '/800x[0-9]+\+0\+[0-9]+/ && \$0 !~ /800x600/ { print \$1; exit }')
xprop -id \$TB _SG_ACRYLIC > "$T/acrylic.on" 2>&1
"$WINE" reg add 'HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize' /v EnableTransparency /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINE" eraschemes-probe.exe notify >/dev/null 2>&1; sleep 2
xprop -id \$TB _SG_ACRYLIC > "$T/acrylic.off" 2>&1
# Alt+Tab's switcher (0746): two windows, Alt held, Tab
"$WINE" taskkill /f /im rounded-probe.exe >/dev/null 2>&1; sleep 1
"$WINE" rounded-probe.exe win One 100 100 300 200 & sleep 2
"$WINE" rounded-probe.exe win Two 200 150 300 200 & sleep 3
xdotool keydown alt; sleep 0.3; xdotool key Tab; sleep 1.5
S=\$(xwininfo -root -tree | awk '/"Task Switching"/ { print \$1; exit }')
xprop -id \$S _SG_SHADOW > "$T/switcher.out" 2>&1
xdotool keyup alt
# minimized, each says where its taskbar button is (0751): One's, then Two's
sleep 1
"$WINE" rounded-probe.exe minimize One; "$WINE" rounded-probe.exe minimize Two; sleep 2
for n in One Two; do
    xprop -id \$(xwininfo -root -tree | awk -v n="\"\$n\":" '\$2 == n { print \$1; exit }') _SG_MINRECT > "$T/minrect.\$n" 2>&1
done
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > "$T/session.out" 2>&1

grep -q 'pixmap id #' "$T/pixmap.out" && pass "the desktop names its picture: $(tr -d '\n' < "$T/pixmap.out")" || fail "no _SG_DESKTOP_PIXMAP: $(cat "$T/pixmap.out")"
[ "$(cat "$T/pixel.out" 2>/dev/null)" = "0,255,0" ] && pass "and the pixmap is alive and shows the desktop (green)" \
    || fail "the desktop's pixmap: $(cat "$T/pixel.out" 2>/dev/null) (freed?)"
viewable() { grep ' IsViewable ' "$T/children.out"; }
viewable | grep '"Framed"' | grep -q '_SG_SHADOW(CARDINAL) = 1' && pass "a window with a title bar: _SG_SHADOW 1" \
    || fail "framed window: $(viewable | grep '"Framed"')"
viewable | grep -q '_SG_SHADOW(CARDINAL) = 2' && pass "a popup menu: _SG_SHADOW 2 (CS_DROPSHADOW)" \
    || fail "menu: $(viewable | grep -v 'Framed' | head -5)"
tb=$(awk '/800x[0-9]+\+0\+[0-9]+/ && $0 !~ /800x600/ { print $1; exit }' "$T/tree.out")
grep "^$tb " "$T/children.out" | grep -q '_SG_SHADOW(CARDINAL) = 0' && pass "the taskbar: _SG_SHADOW 0" \
    || fail "taskbar ($tb): $(grep "^$tb " "$T/children.out")"

grep -q '_SG_SHADOW(CARDINAL) = 2' "$T/switcher.out" && pass "Alt+Tab's switcher asks for a shadow (_SG_SHADOW 2, CS_DROPSHADOW)" \
    || fail "switcher: $(cat "$T/switcher.out")"
grep -q '_SG_ACRYLIC(CARDINAL) = 85' "$T/acrylic.on" && pass "the taskbar is frosted (_SG_ACRYLIC 85) with Transparency effects on" \
    || fail "taskbar acrylic: $(cat "$T/acrylic.on")"
grep -q 'not found' "$T/acrylic.off" && pass "and not once they are turned off" || fail "after turning off: $(cat "$T/acrylic.off")"

# _SG_MINRECT(CARDINAL) = x, y, w, h: on the taskbar (the 40 px at the bottom), One's button before Two's
mr() { sed -n 's/.*= \([0-9-]*\), \([0-9-]*\), \([0-9-]*\), \([0-9-]*\)$/\1 \2 \3 \4/p' "$T/minrect.$1"; }
read -r x1 y1 w1 h1 <<M
$(mr One)
M
read -r x2 y2 w2 h2 <<M
$(mr Two)
M
[ -n "${x1:-}" ] && [ -n "${x2:-}" ] && [ "$y1" -ge 560 ] && [ "$y2" -ge 560 ] && [ "$w1" -gt 0 ] && [ "$h1" -gt 0 ] && [ $((x1 + w1)) -le "$x2" ] \
    && pass "a minimized window tells the compositor where its taskbar button is (_SG_MINRECT: One $x1,$y1 ${w1}x$h1, Two $x2,$y2)" \
    || fail "_SG_MINRECT: One $(cat "$T/minrect.One"), Two $(cat "$T/minrect.Two")"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
