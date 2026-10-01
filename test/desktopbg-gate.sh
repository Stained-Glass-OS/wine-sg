#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Where a window leaves the desktop, the desktop shows at once (patches/sg/0610).
# The virtual desktop's X window had no background, so X left what was there --
# a fast-dragged window left copies of itself -- until explorer repainted.
# explorer now hands winex11 the desktop's picture as that window's background.
# Here explorer and a green probe window are both stopped (SIGSTOP), the
# probe's X window is unmapped behind Wine's back, and the spot must show the
# desktop's magenta at once, not the green.
#
#   WINE=/opt/wine-sg/bin/wine test/desktopbg-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run import xwininfo xdotool; do
    command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-desktopbg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap 'kill -CONT $(cat "$T/pids" 2>/dev/null) 2>/dev/null; "$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -mwindows -o "$T/desktopbg-probe.exe" "$HERE/desktopbg-probe.c" -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/desktopbg-probe.exe" "$WINEPREFIX/drive_c/"
# a plain magenta desktop: no wallpaper, no icons
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '255 0 255' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' /v HideIcons /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
sleep 6
"$WINE" desktopbg-probe.exe &
i=0; while [ \$i -lt 40 ]; do
    w=\$(xwininfo -root -tree | sed -n 's/^ *\(0x[0-9a-f]*\) "SGBGPROBE".*/\1/p' | head -1)
    [ -n "\$w" ] && break
    sleep 1; i=\$((i + 1))
done
sleep 3
echo "\$w" > "$T/probewin"
pids="\$(pgrep -f 'explorer.exe /desktop=shell') \$(pgrep -f 'desktopbg-probe.exe')"
echo \$pids > "$T/pids"
kill -STOP \$pids
import -window root "$T/before.png"
[ -n "\$w" ] && xdotool windowunmap "\$w"
sleep 1
import -window root "$T/after.png"
kill -CONT \$pids
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
[ -s "$T/probewin" ] || { fail "the probe window never showed"; exit 1; }
px() { convert "$1" -format "%[pixel:p{$2,$3}]" info: 2>/dev/null; }
b=$(px "$T/before.png" 400 275); a=$(px "$T/after.png" 400 275)
echo "probe spot before: $b, after unmap: $a"
case "$b" in *0,200,0*|*"0%,78"*) pass "the probe window was on the desktop" ;; *) fail "no green probe before: $b" ;; esac
case "$a" in *255,0,255*|*"100%,0%,100%"*|*magenta*) pass "unmapped with explorer stopped: X shows the desktop at once" ;;
    *) fail "unmapped with explorer stopped: X shows $a, not the desktop (no background)" ;; esac
exit $RC
