#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window focused before it is shown gets the keyboard (patches/sg/0449).
#
# Firefox gives its top-level window the focus, then shows it. X refused the
# focus request -- the window was not on the screen yet -- and winex11 made it
# again on mapping only for a child of the window: X kept the keyboard on the
# desktop's window, and typing into Firefox did nothing until focus moved away
# and back (field report 2). The shell's desktop has the keyboard first here
# (a click on it), as after closing Start.
#
#   WINE=/opt/wine-sg/bin/wine test/focusmap-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-focusmap.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/focusmap-probe.exe" "$HERE/focusmap-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
sleep 8
xdotool mousemove 700 500 click 1; sleep 1
"$WINE" "$T/focusmap-probe.exe" >/dev/null 2>&1 &
sleep 5
xdotool type --delay 80 abc; sleep 2
xdotool search --name '^typed:' getwindowname 2>/dev/null > "$T/title.out"
"$WINE" cmd /c exit >/dev/null 2>&1
EOF
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
title=$(cat "$T/title.out" 2>/dev/null)
echo "      title: $title"
# the window's title is its X window's name only when it is one; ask Wine too
[ "$title" = "typed:abc" ] && pass "typing reaches a window focused before it was shown" || fail "typed into it: '$title'"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
