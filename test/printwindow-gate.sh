#!/bin/sh
# PrintWindow of another process's window (patches/sg/0076).
#
# A window painted green, in its own process; this gate's other process
# PrintWindows it: with WM_PRINT (flags 0), with PW_RENDERFULLCONTENT (what it
# last drew), and PW_RENDERFULLCONTENT once it is cloaked -- off the screen,
# as a window on another virtual desktop is when Task View pictures it.
#
#   WINE=/opt/wine-sg/bin/wine test/printwindow-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-printwindow.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/printwindow-probe.exe" "$HERE/printwindow-probe.c" -ldwmapi -lgdi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/printwindow-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { echo "\$*" >> "$T/log"; "$WINE" printwindow-probe.exe "\$@" 2>/dev/null | tr -d '\r' >> "$T/log"; }
"$WINE" printwindow-probe.exe window Shown >/dev/null 2>&1 &
"$WINE" printwindow-probe.exe window Cloaked cloak >/dev/null 2>&1 &
sleep 5
P print Shown 0
P print Shown 2
P print Cloaked 2
EOF
chmod +x "$T/session.sh"
timeout -s KILL 180 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
sed 's/^/      /' "$T/log"
after() { awk -v c="$1" '$0 == c { getline; print; exit }' "$T/log"; }
[ "$(after 'print Shown 0')" = "ret=1 pixel=0,200,0" ] && pass "PrintWindow of another process's window paints it (WM_PRINT)" || fail "flags 0: $(after 'print Shown 0')"
[ "$(after 'print Shown 2')" = "ret=1 pixel=0,200,0" ] && pass "PW_RENDERFULLCONTENT copies what it drew" || fail "full content: $(after 'print Shown 2')"
[ "$(after 'print Cloaked 2')" = "ret=1 pixel=0,200,0" ] && pass "even cloaked, off the screen" || fail "cloaked: $(after 'print Cloaked 2')"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
