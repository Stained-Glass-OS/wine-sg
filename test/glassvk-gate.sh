#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A popup that is a sheet of glass, drawn by another process with Vulkan, is
# shown over what is under it (patches/sg/0615). Chromium's GPU process
# presents its bubbles into the browser's popups: in the virtual desktop
# they had a black frame where their shadow is (the Vulkan window had no
# alpha, and X drops a child window's alpha anyway). The rendering process
# now gets an offscreen X window with alpha, and blends each frame over the
# backdrop the popup's own process took and names (_SG_BACKDROP, a pixmap).
# Xvfb shows nothing a Vulkan swap chain presents, so the gate checks the two
# halves: the popup over a magenta desktop names a magenta backdrop, and the
# other process's swap chain is in a 32-bit window of the popup's size. (The
# picture itself is checked in the QA VM: Chrome's bubbles.)
#
#   WINE=/opt/wine-sg/bin/wine DXVK=/opt/sg-d3d/dxvk/x64 test/glassvk-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
DXVK="${DXVK:-/opt/sg-d3d/dxvk/x64}"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in xvfb-run xwininfo cc "$MINGWXX"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$DXVK/dxgi.dll" ] && [ -f "$DXVK/d3d11.dll" ] || { echo "SKIP: no DXVK at $DXVK"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-glassvk.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DXVK_LOG_LEVEL=none
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;dxgi,d3d11=n"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGWXX" -O2 -static -o "$T/glassvk-probe.exe" "$HERE/glassvk-probe.cpp" -ld3d11 -ldxgi -ldwmapi -lgdi32 ||
    { fail "probe did not build"; exit 1; }
cc -O2 -o "$T/backdrop-check" "$HERE/backdrop-check.c" -lX11 || { echo "SKIP: cannot build backdrop-check (libx11-dev)"; exit 77; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$DXVK/dxgi.dll" "$DXVK/d3d11.dll" "$WINEPREFIX/drive_c/windows/system32/"
cp "$T/glassvk-probe.exe" "$WINEPREFIX/drive_c/"
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
"$WINE" 'C:\\glassvk-probe.exe' host > "$T/probe.out" 2>/dev/null &
sleep 12
xwininfo -root -tree > "$T/tree.txt"
w=\$(sed -n 's/^ *\(0x[0-9a-f]*\) "glassvk".*/\1/p' "$T/tree.txt" | head -1)
echo "\$w" > "$T/popup"
[ -n "\$w" ] && "$T/backdrop-check" "\$w" > "$T/backdrop.out"
for c in \$(grep -o '0x[0-9a-f]* (has no name): ()  300x200' "$T/tree.txt" | cut -d' ' -f1); do
    xwininfo -id "\$c" | grep -q 'Depth: 32' && echo "\$c" >> "$T/argb.out"
done
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
grep -q 'render=2' "$T/probe.out" 2>/dev/null && { echo "SKIP: no Direct3D 11 device (Vulkan)"; exit 77; }
[ -s "$T/popup" ] && [ "$(cat "$T/popup")" != "" ] || { fail "the glass popup's X window was not found"; exit 1; }
echo "      popup $(cat "$T/popup"): $(cat "$T/backdrop.out" 2>/dev/null)"
grep -q 'pixmap=0x.* 300x200 pixel=ff00ff' "$T/backdrop.out" 2>/dev/null &&
    pass "the popup names its backdrop: a pixmap of the desktop under it (magenta)" ||
    fail "the popup names no backdrop of the desktop: $(cat "$T/backdrop.out" 2>/dev/null)"
[ -s "$T/argb.out" ] && pass "the other process's swap chain is in a 32-bit window of the popup's size ($(head -1 "$T/argb.out"))" ||
    fail "no 32-bit window for the other process's swap chain"
exit $RC
