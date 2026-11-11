#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A top-level window that presents with Vulkan keeps its frame when it is raised (wine-sg 7732). The
# first SetWindowPos after a swap chain was made switched the window from its window surface to drawing
# straight to its X window, and copied the old surface (GDI: white) over the client area -- over the
# swap chain's X window too (IncludeInferiors). The frame was hidden until the program presented again,
# which a still page never does: Viber's elevated copy turned blank when a second copy asked it to come
# forward. No virtual desktop (top-level windows are X windows of their own, as on an elevated
# program's display), no window manager. test/vkraise-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/vkraise-gate.sh
# Mutant: SG_MUTANT_COPY_OVER_CLIENT_SURFACE (dlls/win32u/window.c).
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
T=$(mktemp -d /var/tmp/sg-vkraise.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/inc"; cp -r /usr/include/vulkan "$T/inc/"; [ -d /usr/include/vk_video ] && cp -r /usr/include/vk_video "$T/inc/"
x86_64-w64-mingw32-gcc -O2 -I"$T/inc" -o "$T/vkraise.exe" "$HERE/vkraise-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/vkraise.exe" "$WINEPREFIX/drive_c/"
cat > "$T/session.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" 'C:\\vkraise.exe' > "$T/probe.out" 2>/dev/null &
i=0; while ! grep -q presented= "$T/probe.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i+1)); done
sleep 0.5
import -window root "$T/a.png"
i=0; while ! grep -q raised "$T/probe.out" 2>/dev/null && [ \$i -lt 20 ]; do sleep 0.5; i=\$((i+1)); done
sleep 2
import -window root "$T/b.png"
EOS
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
grep -q 'presented=1' "$T/probe.out" 2>/dev/null || { echo "SKIP: no Vulkan presentation here ($(cat "$T/probe.out" 2>/dev/null))"; exit 77; }
grep -q raised "$T/probe.out" || { fail "the probe did not raise its window"; exit 1; }
px() { convert "$T/$1.png" -format "%[pixel:p{$2}]" info:; }
red='srgb(255,0,0)'
A="$(px a 300,250) $(px a 105,105) $(px a 495,395)"; B="$(px b 300,250) $(px b 105,105) $(px b 495,395)"
echo "      before: $A / after the raise: $B"
[ "$A" = "$red $red $red" ] && pass "the window's Vulkan frame is shown" || fail "the frame was not shown: $A"
[ "$B" = "$red $red $red" ] && pass "after SetWindowPos(HWND_TOP) the frame is still shown (not the old window surface's white)" ||
    fail "after the raise: $B"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
