#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Closing the last window on a virtual desktop stays on that desktop
# (patches/sg/0477). David: "if you close an app and it's the last one on
# the virtual desktop, it automatically switches back to a desktop with an
# open app". Wine gave the foreground to the next window, on another
# desktop, and explorer followed the foreground there. A window brought
# forward on purpose (its taskbar button, Alt+Tab) still switches.
#
#   WINE=/opt/wine-sg/bin/wine test/vdstay-gate.sh
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

T=$(mktemp -d /var/tmp/sg-vdstay.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/vdesk-probe.exe" "$HERE/vdesk-probe.c" -ldwmapi -lgdi32 -lole32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vdesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VirtualDesktops' /v VirtualDesktopIDs \
    /t REG_BINARY /d 5347564431000000a1b2c3d4e5f60718 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" vdesk-probe.exe "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
"$WINE" vdesk-probe.exe window Alpha 60 60 >/dev/null 2>&1 &
sleep 3
P new; sleep 2
"$WINE" vdesk-probe.exe window Beta 300 200 >/dev/null 2>&1 &
sleep 3
P query
P closewin Beta; sleep 3
P query
"$WINESERVER" -k
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" 2>/dev/null
[ "$(sed -n 1p "$T/log.out")" = "desktops=2 current=1" ] || fail "setup: not on the second desktop with Beta"
[ "$(sed -n 2p "$T/log.out")" = "closed=1" ] || fail "Beta was not closed"
[ "$(sed -n 3p "$T/log.out")" = "desktops=2 current=1" ] && pass "closing the last window on a desktop stays on it" \
    || fail "after closing Beta: $(sed -n 3p "$T/log.out") (switched to Alpha's desktop)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
