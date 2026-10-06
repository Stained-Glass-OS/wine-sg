#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An opaque Direct2D DC render target starts from what its DC shows under
# DXVK too (patches/sg/1170). 0807 reads the DC into the target's surface
# with GDI at BeginDraw, but released the surface's DC with an empty dirty
# rectangle -- "nothing drawn". Wine's own Direct3D ignores the rectangle;
# DXVK, every installation's Direct3D, honours it and kept the old
# (black) contents: Paint.NET's menus were black with the names of the
# items that have a shortcut missing, and its "More >>" button blank.
#
# Runs test/dcrt-opaque-probe.cpp with DXVK's d3d11/dxgi as native DLLs
# (SG_DXVK_DIR, else /opt/sg-d3d's or a sibling sg-image's staged payload)
# on lavapipe when it is there. SKIP without DXVK or a Vulkan device.
#
#   WINE=/opt/wine-sg/bin/wine test/dcrtdxvk-gate.sh   (mutant SG_MUTANT_DCRT_EMPTY_DIRTY)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
CXX="${CXX:-x86_64-w64-mingw32-g++}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$CXX" >/dev/null || { echo "SKIP: $CXX missing"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing (Direct2D needs a display)"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
DXVK=""
for d in "${SG_DXVK_DIR:-}" /opt/sg-d3d/dxvk/x64 "$HERE/../../sg-image/build/d3d-payload/dxvk/x64" \
         "${SG_REAL_HOME:-/nonexistent}/Stained-Glass-OS/sg-image/build/d3d-payload/dxvk/x64"; do
    [ -n "$d" ] && [ -f "$d/d3d11.dll" ] && [ -f "$d/dxgi.dll" ] && [ -f "$d/d3d10core.dll" ] && { DXVK=$d; break; }
done
[ -n "$DXVK" ] || { echo "SKIP: no DXVK payload (SG_DXVK_DIR=.../dxvk/x64, sg-image make d3d)"; exit 77; }
[ -z "${VK_ICD_FILENAMES:-}" ] && [ -f /usr/share/vulkan/icd.d/lvp_icd.json ] && export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
unset WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-dcrtdxvk.XXXXXX)
n=180; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=-all DXVK_LOG_LEVEL=none WINESERVER DISPLAY=":$n"
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;d3d11,dxgi,d3d10core=n"
trap '"$WINESERVER" -k 2>/dev/null; kill $XP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$CXX" -O2 -static -o "$T/probe.exe" "$HERE/dcrt-opaque-probe.cpp" -ld2d1 -lgdi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$DXVK/d3d11.dll" "$DXVK/dxgi.dll" "$DXVK/d3d10core.dll" "$WINEPREFIX/drive_c/windows/system32/"
timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
grep -q '^NOFACTORY\|^NORT' "$T/o" && { echo "SKIP: no Direct2D target over DXVK here (no Vulkan device?)"; cat "$T/o"; exit 77; }
line() { grep "^$1 " "$T/o" | head -1; }
for via in SELF CTX; do
    l=$(line "OPAQUE $via")
    case "$l" in *"keep=7030c0 ink=ff0000"*) pass "DXVK, opaque target, $via: the DC's colour stays where nothing was drawn";;
                 *) fail "DXVK, opaque $via: '$l'";; esac
    l=$(line "OPAQUE ${via}2")
    case "$l" in *"first=7030c0 keep=7030c0 ink=00ff00"*) pass "DXVK, opaque target, $via, drawn again: the DC shows through";;
                 *) fail "DXVK, opaque ${via}2: '$l'";; esac
    l=$(line "PREMUL ${via}2")
    case "$l" in *"first=7030c0 keep=7030c0 ink=00ff00"*) pass "DXVK, premultiplied target, $via, drawn again: no old ink";;
                 *) fail "DXVK, premultiplied ${via}2: '$l'";; esac
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; cat "$T/o"; }
exit "$RC"
