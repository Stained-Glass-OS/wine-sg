#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Drawing into a window with alpha (wine-sg 7786). A top-level that is a sheet of DWM glass or
# layered has an X window of 32 bits (bitblt.c); a child's Vulkan or OpenGL frame (24 bits) was
# put there with XCopyArea, and another process drawing through the child's DC drew as 24 bits:
# BadMatch, an X error that ends the process. DevToys 2 (WPF with Mica and a WebView2 control):
# its renderer and Chromium's GPU process died at the first frame, the window stayed black.
# test/glassdepth-probe.c, in the shell under Xvfb, for a glass and a layered top-level:
#   vk   a child presents red with Vulkan            -- shown, the process lives
#   gl   a child presents green with OpenGL          -- shown, the process lives
#   gdi  another process fills the child blue through its DC (DXVK's composition swap chains)
#                                                    -- shown, both processes live
#   WINE=/opt/wine-sg/bin/wine test/glassdepth-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in xvfb-run import convert x86_64-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -f /usr/include/vulkan/vulkan.h ] || { echo "SKIP: no Vulkan headers (libvulkan-dev)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-glassdepth.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
mkdir -p "$T/inc"; cp -r /usr/include/vulkan "$T/inc/"; [ -d /usr/include/vk_video ] && cp -r /usr/include/vk_video "$T/inc/"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -I"$T/inc" -o "$T/glassdepth.exe" "$HERE/glassdepth-probe.c" -lgdi32 -lopengl32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/glassdepth.exe" "$WINEPREFIX/drive_c/"
cat > "$T/session.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
sleep 6
for c in "vk glass" "gl glass" "gdi glass" "vk layered" "gdi layered"; do
    set -- \$c
    n=\$1-\$2
    "$WINE" 'C:\\glassdepth.exe' \$1 \$2 > "$T/\$n.out" 2> "$T/\$n.err" &
    sleep 4
    import -window root "$T/\$n.png"
    sleep 14
done
EOS
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
grep -q 'vk=1' "$T/vk-glass.out" 2>/dev/null || grep -q 'BadMatch' "$T/vk-glass.err" 2>/dev/null ||
    { echo "SKIP: no Vulkan presentation here ($(cat "$T/vk-glass.out" 2>/dev/null))"; exit 77; }
px() { convert "$T/$1.png" -format "%[pixel:p{250,225}]" info: 2>/dev/null; }
check() {   # CASE MODE COLOUR WHAT
    out=$(tr -d '\r' < "$T/$1.out" 2>/dev/null | tr '\n' ' ')
    if grep -q 'X Error' "$T/$1.err" 2>/dev/null; then
        fail "$4: an X error ended a process: $(grep -A2 'X Error' "$T/$1.err" | tr '\n' ' ' | cut -c1-160)"
    else pass "$4: no X error"; fi
    echo "$out" | grep -q "$2=1" && echo "$out" | grep -q 'alive=1' && pass "$4: drawn, and the process lives on" \
        || fail "$4: output '$out'"
    p=$(px "$1")
    [ "$p" = "$3" ] && pass "$4: the frame is shown ($p)" || fail "$4: at the child's centre $p, wanted $3"
}
check vk-glass vk 'srgb(255,0,0)' "glass, Vulkan"
check gl-glass gl 'srgb(0,255,0)' "glass, OpenGL"
check gdi-glass gdi 'srgb(0,0,255)' "glass, another process through the DC"
check vk-layered vk 'srgb(255,0,0)' "layered, Vulkan"
check gdi-layered gdi 'srgb(0,0,255)' "layered, another process through the DC"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
