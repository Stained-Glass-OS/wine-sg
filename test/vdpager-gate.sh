#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's desktop pager and Task View button (patches/sg/0446).
#
# A new account starts with four desktops; the pager (a box per desktop, the
# current one in the accent colour, windows drawn as outlines) sits at the
# right of the bar with the Task View button just left of it; clicking a box
# goes there, the wheel steps, the right button offers a new desktop; the
# accent follows Personalization (DWM AccentColor); and the Task View button
# still opens Task View after it was dismissed and the desktop switched by
# keys (it did not: a stale window handle).
#
#   WINE=/opt/wine-sg/bin/wine test/vdpager-gate.sh
#   ARTIFACTS=DIR keeps screenshots and logs
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run, ImageMagick and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-vdpager.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { echo "\$*" >> "$T/log.out"; "$WINE" vdesk-probe.exe "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
shot() { sleep 1.5; import -window root "$T/\$1.png"; }
# the centre of box I of N in the pager (x) and the bar's middle (y)
box() { r=\$("$WINE" vdesk-probe.exe pager 2>/dev/null | tr -d '\r' | sed 's/^pager=\([^ ]*\) .*/\1/'); echo "\$r" | awk -F, -v i="\$1" -v n="\$2" '{ printf "%d %d\n", \$1 + (\$3 - \$1) * (2 * i + 1) / (2 * n), (\$2 + \$4) / 2 }'; }
tv() { "$WINE" vdesk-probe.exe pager 2>/dev/null | tr -d '\r' | sed 's/.*taskview=//' | awk -F, '{ printf "%d %d\n", (\$1 + \$3) / 2, (\$2 + \$4) / 2 }'; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
"$WINE" vdesk-probe.exe window Alpha 60 60 >/dev/null 2>&1 &
sleep 3
P query; P pager; shot start
set -- \$(box 0 4); echo "box0 \$1 \$2" >> "$T/log.out"
set -- \$(box 2 4); xdotool mousemove \$1 \$2 click 1; sleep 2
P query; P state Alpha; shot third
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\DWM' /v AccentColor /t REG_DWORD /d 0xff3e8910 /f >/dev/null 2>&1
sleep 1.5; xdotool mousemove 500 300; sleep 1.5; shot green
set -- \$(box 2 4); xdotool mousemove \$1 \$2 click 5; sleep 2
P query
set -- \$(box 0 4); xdotool mousemove \$1 \$2 click 3; sleep 1.5; shot menu; xdotool key n; sleep 2
P query; P pager
# Task View from its button; dismissed; a desktop switch by keys; the button again
set -- \$(tv); xdotool mousemove \$1 \$2 click 1; sleep 2
P exists SgTaskView; shot taskview
xdotool key Escape; sleep 1
P exists SgTaskView
xdotool key super+ctrl+Left; sleep 2
set -- \$(tv); xdotool mousemove \$1 \$2 click 1; sleep 2
P exists SgTaskView; shot taskview2
xdotool key Escape; sleep 1
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out"

after() { awk -v c="$1" -v n="${2:-1}" '$0 == c { k++; if (k == n) { getline; print; exit } }' "$T/log.out"; }
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
rect() { after pager "$1" | sed "s/.*$2=//; s/ .*//"; }

[ "$(after query 1)" = "desktops=4 current=0" ] && pass "a new account has four desktops" || fail "start: $(after query 1)"
P=$(rect 1 pager); V=$(rect 1 taskview)
pl=${P%%,*}; pr=$(echo "$P" | cut -d, -f3); pt=$(echo "$P" | cut -d, -f2); pb=$(echo "$P" | cut -d, -f4)
vr=$(echo "$V" | cut -d, -f3)
{ [ "$pr" -gt 0 ] && [ "$pl" -gt 600 ] && [ "$pt" -ge 650 ]; } && pass "the pager is on the bar, at its right ($P)" || fail "pager at $P"
[ "$vr" = "$pl" ] && pass "the Task View button is just left of it ($V)" || fail "Task View button at $V, pager at $P"
my=$(( (pt + pb) / 2 )); bw=$(( (pr - pl) / 4 ))
# box 0 holds Alpha's outline at its left; its right is the desktop in the accent
x0=$(( pl + bw - 5 )); x1=$(( pl + bw + bw / 2 ))
[ "$(px start $x0 $my)" = "138,43,226" ] && pass "the current desktop's box is in the accent colour" || fail "box 0 at $x0,$my: $(px start $x0 $my)"
[ "$(px start $x1 $my)" != "138,43,226" ] && pass "and the others are not" || fail "box 1 is in the accent too"
ax=$(( pl + 11 )); ay=$(( pt + 13 ))  # Alpha (60,60 360x260 of 1024x700), in a 35x24 box at +4,+8
[ "$(px start $ax $ay)" = "244,244,244" ] && pass "Alpha is drawn in box 1" || fail "no outline of Alpha at $ax,$ay: $(px start $ax $ay)"

[ "$(after query 2)" = "desktops=4 current=2" ] && pass "clicking the third box goes to desktop 3" || fail "click: $(after query 2)"
case "$(after 'state Alpha' 1)" in "visible=1 cloaked=0x2 desktop=0") pass "and Alpha stays behind on desktop 1" ;; *) fail "Alpha: $(after 'state Alpha' 1)" ;; esac
x2=$(( pl + 2 * bw + bw / 2 ))
[ "$(px third $x2 $my)" = "138,43,226" ] && pass "box 3 is now the accented one" || fail "box 3: $(px third $x2 $my)"
[ "$(px green $x2 $my)" = "16,137,62" ] && pass "the accent follows Personalization (green)" || fail "accent: $(px green $x2 $my)"
[ "$(after query 3)" = "desktops=4 current=3" ] && pass "the wheel steps to the next desktop" || fail "wheel: $(after query 3)"
[ "$(after query 4)" = "desktops=5 current=3" ] && pass "right button > New desktop adds one" || fail "menu: $(after query 4)"
P2=$(rect 2 pager); [ "$(( $(echo "$P2" | cut -d, -f3) - ${P2%%,*} ))" -gt "$(( pr - pl ))" ] && pass "and the pager grows for it" || fail "pager after new: $P2 (was $P)"

[ "$(after 'exists SgTaskView' 1)" = "exists=1" ] && pass "the Task View button opens Task View" || fail "Task View did not open"
[ "$(after 'exists SgTaskView' 2)" = "exists=0" ] && pass "Escape closes it" || fail "Escape did not close it"
[ "$(after 'exists SgTaskView' 3)" = "exists=1" ] && pass "and the button opens it again after a switch by keys" || fail "Task View did not open again"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
