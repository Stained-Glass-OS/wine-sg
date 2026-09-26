#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DWM cloaking (patches/sg/0067), the primitive virtual desktops are built on.
#
# A program cloaks its own window with DwmSetWindowAttribute(DWMWA_CLOAK), as
# on Windows. The gate requires that the window stays visible to the program
# (IsWindowVisible, WS_VISIBLE) and is sent no show or position messages,
# but is gone from the screen -- checked on the X server's own pixels -- with
# the window beneath repainted and hit-tested instead; that another process
# reads DWMWA_CLOAKED; that moving it while cloaked keeps it unseen; and that
# uncloaking brings it back.
#
#   WINE=/opt/wine-sg/bin/wine test/cloak-gate.sh
#   ARTIFACTS=DIR keeps the screenshots and the probe's output
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

T=$(mktemp -d /var/tmp/sg-cloak.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/cloak-probe.exe" "$HERE/cloak-probe.c" -ldwmapi -lgdi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX" "$T/sync"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/cloak-probe.exe" "$WINEPREFIX/drive_c/"
# programs join the shell's desktop, as in a session (sg-session's sg-run-explorer)
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w
SYNC=$("$WINE" winepath -w "$T/sync" 2>/dev/null)

cat > "$T/session.sh" <<EOF
#!/bin/sh
"$WINE" explorer /desktop=shell,800x600 >/dev/null 2>&1 &
sleep 5
(cd "$WINEPREFIX/drive_c" && WINEDEBUG=err+all "$WINE" cloak-probe.exe blue > "$T/blue.out" 2>&1 &)
sleep 3
cd "$WINEPREFIX/drive_c" && "$WINE" cloak-probe.exe run '$SYNC' > "$T/probe.out" 2>/dev/null &
for s in shown cloaked moved uncloaked; do
    i=0
    while [ "\$(cat "$T/sync/step.txt" 2>/dev/null)" != "\$s" ] && [ \$i -lt 600 ]; do sleep 0.1; i=\$((i + 1)); done
    import -window root "$T/\$s.png"
    printf '%s' "\$s" > "$T/sync/ack.txt"
done
sleep 1
EOF
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
cat "$T/probe.out" 2>/dev/null | sed 's/^/      /'

# the colour of the screen at a point, from the X server's pixels
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
line() { grep "^$1 " "$T/probe.out" | tr -d '\r'; }
field() { line "$1" | tr -d '\r' | tr ' ' '\n' | sed -n "s/^$2=//p"; }

[ "$(px shown 300 245)" = "255,0,0" ] && pass "the red window is on the screen" || fail "red window not drawn: $(px shown 300 245)"
[ "$(field shown from_point)" = red ] || fail "red window not hit-tested before cloaking"
grep -q '^set_cloak hr=0' "$T/probe.out" && pass "DwmSetWindowAttribute(DWMWA_CLOAK) succeeds" || fail "DWMWA_CLOAK: $(grep set_cloak "$T/probe.out")"
[ "$(field cloaked visible)" = 1 ] && [ "$(field cloaked style_visible)" = 1 ] \
    && pass "cloaked, it is still visible to the program (IsWindowVisible, WS_VISIBLE)" || fail "cloaking changed visibility: $(line cloaked)"
[ "$(field cloaked showmsgs)" = 0 ] && [ "$(field cloaked posmsgs)" = 0 ] \
    && pass "and it was sent no WM_SHOWWINDOW or WM_WINDOWPOS* messages" || fail "messages sent on cloak: $(line cloaked)"
[ "$(field cloaked cloaked)" = 0x1 ] && pass "DWMWA_CLOAKED reports DWM_CLOAKED_APP" || fail "DWMWA_CLOAKED: $(line cloaked)"
grep -q '^query cloaked=0x1 hr=0' "$T/probe.out" && pass "another process reads DWMWA_CLOAKED too" || fail "cross-process DWMWA_CLOAKED: $(grep '^query' "$T/probe.out")"
[ "$(px cloaked 300 245)" = "0,0,255" ] && pass "it is gone from the screen; the window beneath repainted there" || fail "cloaked window still on screen: $(px cloaked 300 245)"
[ "$(field cloaked from_point)" = blue ] && pass "and clicks there reach the window beneath" || fail "cloaked window still hit-tested: $(line cloaked)"
[ "$(px moved 250 225)" = "0,0,255" ] && [ "$(field moved visible)" = 1 ] \
    && pass "moved while cloaked, it stays unseen" || fail "moved cloaked window: $(px moved 250 225) $(line moved)"
grep -q '^set_uncloak hr=0' "$T/probe.out" || fail "uncloak call failed"
[ "$(px uncloaked 250 225)" = "255,0,0" ] && [ "$(field uncloaked from_point)" = red ] && [ "$(field uncloaked cloaked)" = 0 ] \
    && pass "uncloaked, it is back where it was moved to, drawn and hit-tested" || fail "uncloak: $(px uncloaked 250 225) $(line uncloaked)"
[ "$(field uncloaked showmsgs)" = 0 ] && pass "uncloaking sent no WM_SHOWWINDOW" || fail "uncloak sent WM_SHOWWINDOW: $(line uncloaked)"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
