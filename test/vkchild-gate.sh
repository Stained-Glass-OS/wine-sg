#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A child window that presents with Vulkan keeps its picture when its
# top-level is exposed (patches/sg/1523). Office draws its panes with
# Direct3D (DXVK, so Vulkan) into child windows. Their frames are rendered
# offscreen and copied into the top-level's X window; the top-level's GDI
# surface (white, for Office's panes) was put over them at the next flush --
# a dialog closing over the window, or painting nearby. Windows' compositor
# keeps a child swap chain's last frame: Excel's formula bar, sheet tabs and
# status bar, Outlook's panes, whole Office windows stayed white. The probe
# clears a child to red once; a window then covers it for a second and goes.
#
#   WINE=/opt/wine-sg/bin/wine test/vkchild-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in xvfb-run import convert x86_64-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -f /usr/include/vulkan/vulkan.h ] || { echo "SKIP: no Vulkan headers (libvulkan-dev)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-vkchild.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/inc"; cp -r /usr/include/vulkan "$T/inc/"; [ -d /usr/include/vk_video ] && cp -r /usr/include/vk_video "$T/inc/"
x86_64-w64-mingw32-gcc -O2 -I"$T/inc" -o "$T/vkchild.exe" "$HERE/vkchild-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Control Panel\Desktop' /v Wallpaper /d '' /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vkchild.exe" "$WINEPREFIX/drive_c/"
cat > "$T/session.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
sleep 6
"$WINE" 'C:\\vkchild.exe' > "$T/probe.out" 2>/dev/null &
sleep 8
import -window root "$T/a.png"
"$WINE" 'C:\\vkchild.exe' cover
sleep 3
import -window root "$T/b.png"
"$WINE" 'C:\\vkchild.exe' nest > "$T/nest.out" 2>/dev/null &
sleep 8
"$WINE" 'C:\\vkchild.exe' cover
sleep 3
import -window root "$T/c.png"
EOS
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
grep -q 'presented=1' "$T/probe.out" 2>/dev/null || { echo "SKIP: no Vulkan presentation here ($(cat "$T/probe.out" 2>/dev/null))"; exit 77; }
px() { convert "$T/$1.png" -format "%[pixel:p{$2}]" info:; }
red='srgb(255,0,0)'; white='srgb(255,255,255)'
A="$(px a 250,225) $(px a 151,151) $(px a 348,298)"; B="$(px b 250,225) $(px b 151,151) $(px b 348,298)"
O="$(px b 148,148) $(px b 120,120)"
echo "      shown: $A / after the cover went: $B / around: $O"
[ "$A" = "$red $red $red" ] && pass "the child's Vulkan frame is shown" || fail "the frame was not shown: $A"
[ "$B" = "$red $red $red" ] && pass "after a window covered it and went, the frame is shown again (not the top-level's white)" ||
    fail "after the cover: $B"
[ "$O" = "$white $white" ] && pass "only the child's rectangle is drawn again" || fail "around the child: $O"
# 1524: a window inside the child (at 170,170 on the screen, 60x40) presented
# green before the child presented red; after an expose the inner frame is
# put back over the outer one, not under it
N="$(px c 200,190)"; NR="$(px c 290,250)"
echo "      nested: inner $N, outer $NR"
[ "$N" = 'srgb(0,255,0)' ] && pass "a window's frame stays over its parent's after an expose" || fail "nested inner frame: $N"
[ "$NR" = "$red" ] && pass "the parent's frame is put back around it" || fail "nested outer frame: $NR"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
