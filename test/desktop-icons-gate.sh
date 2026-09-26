#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell's desktop icons (patches/sg/0080... see CLAUDE.md), typed and clicked on X.
#
# With HideDesktopIcons\NewStartPanel showing This PC and the user's files
# (and the Recycle Bin, shown by default), the desktop draws three icons at
# the top left, their titles shadowed so they read on a wallpaper; a
# double-click on This PC opens File Explorer.
#
#   WINE=/opt/wine-sg/bin/wine test/desktop-icons-gate.sh    (ARTIFACTS=DIR keeps the screenshot)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run, xdotool, ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-desktopicons.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
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
K='HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\HideDesktopIcons\NewStartPanel'
"$WINE" reg add "$K" /v '{20D04FE0-3AEA-1069-A2D8-08002B30309D}' /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINE" reg add "$K" /v '{59031a47-3f72-44a7-89c5-5595fe6b30ee}' /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
# the desktop is drawn once something is on it: a window, away from the icons
"$WINE" vdesk-probe.exe window Away 600 380 >/dev/null 2>&1 &
sleep 4
import -window root "$T/desktop.png"
# the first icon's middle: This PC
xdotool mousemove 50 22 click --repeat 2 --delay 80 1; sleep 5
"$WINE" vdesk-probe.exe find ExplorerWClass 2>/dev/null | tr -d '\r' > "$T/explorer-window.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"

# icons: non-background pixels in each launcher cell of the first column
cells=$(python3 - "$T/desktop.png" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
bg = im.getpixel((300, 600))
out = []
for top in range(0, 700, 70):
    n = sum(1 for x in range(4, 90) for y in range(top + 2, top + 44) if im.getpixel((x, y)) != bg)
    out.append(n)
print(' '.join(str(n) for n in out[:5]))
EOF
)
set -- $cells
[ "${1:-0}" -gt 100 ] && [ "${2:-0}" -gt 100 ] && [ "${3:-0}" -gt 100 ] && [ "${4:-0}" -lt 20 ] \
    && pass "three icons on the desktop: This PC, the user's files, the Recycle Bin" || fail "icons per cell: $cells"
dark=$(python3 - "$T/desktop.png" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
print(sum(1 for x in range(4, 90) for y in range(44, 68) if sum(im.getpixel((x, y))) < 60))
EOF
)
[ "${1:-0}" -gt 100 ] && [ "${dark:-0}" -gt 20 ] && [ "${dark:-0}" -lt 1500 ] && pass "their titles have a shadow" || fail "no title shadow ($dark dark pixels)"
[ "$(tr -d '\r' < "$T/explorer-window.out")" = "found=1" ] && pass "double-clicking This PC opens File Explorer" || fail "no File Explorer window"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
