#!/bin/sh
# A glass popup's frame presented with Vulkan (Chromium's bubbles, Firefox's
# menus on a GPU) keeps its see-through edge (patches/sg/0753). winex11
# blends each frame over the popup's backdrop -- but with the desktop's
# compositor (sg-deskcomp) running it takes no backdrop, and the frame was
# blended over nothing: the edge came out opaque black (David 2026-10-01:
# Firefox's menus had a black border). With no backdrop and a window with
# alpha, the frame is put as it is, for the compositor to blend.
# Xvfb shows nothing a Vulkan swap chain presents, so this checks
# glass_pixel() itself, compiled out of the patched tree (WINE_SRC, else
# tools/lint-tree.sh's).
#
#   test/glasspixel-gate.sh [--mutant]
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v cc >/dev/null || { echo "SKIP: no cc"; exit 77; }
SRC="${WINE_SRC:-$("$HERE/../tools/lint-tree.sh" | tail -1)}"
F="$SRC/dlls/winex11.drv/vulkan.c"
[ -f "$F" ] || { echo "SKIP: no $F"; exit 77; }
T=$(mktemp -d /var/tmp/sg-glasspixel.XXXXXX)
trap 'rm -rf "$T"' EXIT INT TERM
{
    printf '#include <stdio.h>\ntypedef unsigned int DWORD; typedef int BOOL;\n#define min(a, b) ((a) < (b) ? (a) : (b))\n'
    sed -n '/^static inline DWORD glass_pixel(/,/^}/p' "$F"
    cat <<'C'
int main(void)
{
    DWORD edge = 0x40101010, solid = 0xff336699, magenta = 0xffff00ff;
    printf("nobackdrop_argb=%08x\n", glass_pixel(edge, NULL, 1));
    printf("nobackdrop_rgb=%08x\n", glass_pixel(edge, NULL, 0));
    printf("backdrop=%08x\n", glass_pixel(edge, &magenta, 1));
    printf("solid=%08x\n", glass_pixel(solid, NULL, 1));
    return 0;
}
C
} > "$T/t.c"
grep -q 'glass_pixel(' "$T/t.c" || { fail "no glass_pixel() in $F"; exit 1; }
cc -O2 ${MUTANT:+-DSG_MUTANT_GLASS_VK_BLACK} -o "$T/t" "$T/t.c" || { fail "glass_pixel() did not build"; exit 1; }
"$T/t" > "$T/out"
v() { sed -n "s/^$1=//p" "$T/out"; }
[ "$(v nobackdrop_argb)" = 40101010 ] && pass "no backdrop, a window with alpha: the see-through edge is kept (40101010), not black" \
    || fail "no backdrop, window with alpha: $(v nobackdrop_argb) (ff... is the black border)"
[ "$(v nobackdrop_rgb)" = ff101010 ] && pass "no backdrop, no alpha in the window: opaque, as before" || fail "no backdrop, no alpha: $(v nobackdrop_rgb)"
[ "$(v backdrop)" = ffcf10cf ] && pass "with a backdrop: blended over it, opaque (ffcf10cf)" || fail "over the backdrop: $(v backdrop)"
[ "$(v solid)" = ff336699 ] && pass "an opaque pixel stays as it is" || fail "opaque: $(v solid)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
