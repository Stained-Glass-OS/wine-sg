#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# After the resolution changes (Display settings), the taskbar is on the new
# screen's edge with its notification icons beside the clock
# (patches/sg/0481). WM_DISPLAYCHANGE hid the shell's taskbar -- it looked at
# show_systray, the stand-alone tray's flag, false in the shell -- and the
# icons were placed only when added, so they stayed where the old bar ended.
#
#   WINE=/opt/wine-sg/bin/wine test/dispchange-gate.sh
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
T=$(mktemp -d /var/tmp/sg-dispchange.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/dispchange-probe.exe" "$HERE/dispchange-probe.c" -lshell32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dispchange-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { "$WINE" dispchange-probe.exe "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
"$WINE" dispchange-probe.exe icon >/dev/null 2>&1 &
sleep 3
P tray
P change 1280 800; sleep 3
P tray
"$WINESERVER" -k
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1600x1000x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" 2>/dev/null
set -- $(sed -n 3p "$T/log.out")
# tray VISIBLE L T R B icon X Y screen W H
[ "${2:-}" = 1 ] && pass "the taskbar is shown after the change" || fail "the taskbar is hidden after the change"
[ "${4:-}" = 760 ] && [ "${5:-}" = 1280 ] && [ "${6:-}" = 800 ] && pass "on the new screen's bottom edge" || fail "taskbar at ${3:-?},${4:-?},${5:-?},${6:-?}"
[ "${8:-0}" -gt $(( ${11:-0} - 300 )) ] 2>/dev/null && [ "${8:-0}" -lt "${11:-0}" ] 2>/dev/null && [ "${9:-0}" -gt 760 ] 2>/dev/null && pass "its notification icon beside the clock (x ${8})" || fail "the icon is at ${8:-?},${9:-?}"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
