#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An alpha-layered popup shows what is under it through its see-through
# pixels (patches/sg/0611). In a virtual desktop every window is a child of the
# desktop's X window and X does not blend a child's alpha: Chrome's ⋮ menu had
# a thick black frame where its shadow is (David). A half-transparent red
# popup over a magenta desktop must show the blend (255,0,127), not dark red.
#
#   WINE=/opt/wine-sg/bin/wine test/layeredblend-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run import convert; do
    command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-layeredblend.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -mwindows -o "$T/layeredblend-probe.exe" "$HERE/layeredblend-probe.c" -lgdi32 -luser32 ||
    { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/layeredblend-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Colors' /v Background /d '255 0 255' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' /v HideIcons /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
sleep 6
"$WINE" layeredblend-probe.exe &
sleep 6
import -window root "$T/shot.png"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
px=$(convert "$T/shot.png" -format "%[fx:int(255*p{400,275}.r)],%[fx:int(255*p{400,275}.g)],%[fx:int(255*p{400,275}.b)]" info: 2>/dev/null)
bg=$(convert "$T/shot.png" -format "%[fx:int(255*p{100,500}.r)],%[fx:int(255*p{100,500}.g)],%[fx:int(255*p{100,500}.b)]" info: 2>/dev/null)
echo "desktop: $bg, popup: $px"
[ "$bg" = "255,0,255" ] && pass "the desktop is magenta" || fail "the desktop is not magenta: $bg"
r=${px%%,*}; rest=${px#*,}; g=${rest%%,*}; b=${rest#*,}
if [ "${r:-0}" -ge 240 ] && [ "${g:-255}" -le 15 ] && [ "${b:-0}" -ge 110 ] && [ "${b:-0}" -le 145 ]; then
    pass "the half-transparent popup shows the desktop through it ($px)"
else
    fail "the popup shows $px, not red over magenta (255,0,127): its alpha is dropped"
fi
exit $RC
