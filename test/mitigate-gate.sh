#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Process mitigation policies enforced (patches/sg/1705), 64- and 32-bit:
# test/mitigate-probe.c sets each policy in a child of its own and checks the
# process is held to it -- strict handle checks raise, Arbitrary Code Guard
# refuses dynamic code (and lets an opted-out thread), PreferSystem32Images
# takes the system directory's DLL (test/mitigate-dll.c, built twice), non-
# system fonts are refused, and another process's hook DLL is kept out with
# extension points disabled. The policies were kept, not enforced.
#
#   WINE=/opt/wine-sg/bin/wine test/mitigate-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_STRICT_HANDLES (ntdll/unix/server.c), SG_MUTANT_NO_ACG
# (ntdll/unix/virtual.c), SG_MUTANT_FONTS_ALLOWED (win32u/font.c),
# SG_MUTANT_HOOKS_INJECTED (user32/hook.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
FONT=$(ls "$(dirname "$WINE")"/../share/wine/fonts/*.ttf "$(dirname "$WINE")"/fonts/*.ttf 2>/dev/null | head -1)
[ -n "$FONT" ] || { echo "SKIP: no TrueType font beside $WINE"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-mitigate.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    d="$T/$a"; mkdir -p "$d"
    case $a in x86_64) sys="$WINEPREFIX/drive_c/windows/system32";; *) sys="$WINEPREFIX/drive_c/windows/syswow64";; esac
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$d/mitigate-probe.exe" "$HERE/mitigate-probe.c" -lgdi32 -luser32 &&
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -shared -DSGIMG_ID=1 -o "$d/sgimg.dll" "$HERE/mitigate-dll.c" &&
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -shared -DSGIMG_ID=2 -o "$sys/sgimg.dll" "$HERE/mitigate-dll.c" &&
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -shared -Wl,--kill-at -o "$d/sghook.dll" "$HERE/mitigate-dll.c" -luser32 \
        || { echo "FAIL  probe did not build"; exit 1; }
    cp "$FONT" "$d/sgfont.ttf"
    cp "$FONT" "$WINEPREFIX/drive_c/windows/Fonts/sgsys.ttf"
done
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T/$a" && timeout -s KILL 300 env DISPLAY= "$WINE" "$T/$a/mitigate-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
