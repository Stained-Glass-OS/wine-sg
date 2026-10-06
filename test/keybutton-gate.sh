#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar's menu has "Show touch keyboard button" (patches/sg/1151), as
# Windows 10's: a computer without a keyboard needs a way to bring the
# keyboard up. It writes Windows' TipbandDesiredVisibility (HKCU\Software\
# Microsoft\TabletTip\1.7), which sg-shell's touch keyboard reads for its
# button in the notification area; not set, it is on for a computer with a
# touch screen or pen and off for one without (Xvfb has neither). Turned on,
# the touch keyboard is started (TabTip.exe /background, through App Paths:
# a stand-in here) when it is not running.
#
#   WINE=/opt/wine-sg/bin/wine test/keybutton-gate.sh
#
# Mutant: SG_MUTANT_NO_KEYBOARD_BUTTON (explorer systray.c) fails it.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-keybutton.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -mwindows -o "$T/tabtip.exe" "$HERE/keybutton-probe.c" || { fail "probe did not build"; exit 1; }
"$MINGW" -O2 -o "$T/traymenu-probe.exe" "$HERE/traymenu-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/tabtip.exe" "$T/traymenu-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\TabTip.exe' /ve /d 'C:\tabtip.exe' /f >/dev/null 2>&1
"$WINESERVER" -w

TIP='HKCU\Software\Microsoft\TabletTip\1.7'
cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
Q() { echo "\$("$WINE" reg query "$TIP" /v TipbandDesiredVisibility 2>/dev/null | tr -d '\r' | awk '/REG_DWORD/ {print \$3}')" >> "$T/log.out"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
xdotool mousemove 640 680 click 3; sleep 2; "$WINE" traymenu-probe.exe menu 2>/dev/null | tr -d '\r' >> "$T/log.out"
xdotool key y; sleep 3; Q
xdotool mousemove 640 680 click 3; sleep 2; xdotool key y; sleep 2; Q
"$WINESERVER" -k
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out" 2>/dev/null
n() { sed -n "$1p" "$T/log.out" 2>/dev/null; }
[ "$(n 1)" = "menu shown" ] || fail "no menu on the bar: $(n 1)"
[ "$(n 2)" = "0x1" ] && pass "\"Show touch keyboard button\" (off on a computer without touch) turns it on (TipbandDesiredVisibility 1)" \
    || fail "the menu's keyboard button: '$(n 2)'"
grep -q '^started /background' "$WINEPREFIX/drive_c/tabtip.log" 2>/dev/null \
    && pass "and starts the touch keyboard (TabTip.exe /background) for its button" \
    || fail "the touch keyboard was not started: $(cat "$WINEPREFIX/drive_c/tabtip.log" 2>/dev/null)"
[ "$(n 3)" = "0x0" ] && pass "chosen again, it turns the button off" || fail "turning it off: '$(n 3)'"
[ "$(grep -c '^started' "$WINEPREFIX/drive_c/tabtip.log" 2>/dev/null)" = 1 ] && pass "(started once: not when turned off)" \
    || fail "started $(grep -c '^started' "$WINEPREFIX/drive_c/tabtip.log" 2>/dev/null) times"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
