#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# shell32: CSIDL_COMMON_FAVORITES is the caller's own Favorites, as on Windows Vista and later (patches/sg/9102):
# in a shared prefix it was the SYSTEM account's folder (HKLM Common Favorites), which people could not use.
#
#   WINE=/opt/wine-sg/bin/wine test/commonfav-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_COMMON_FAVORITES_HKLM (dlls/shell32/shellpath.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-commonfav.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/commonfav-probe.c" -lshell32 -lole32 -luuid  &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/commonfav-probe.c" -lshell32 -lole32 -luuid  ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
# as the shared prefix has it: the account that built the prefix (SYSTEM) wrote its own
# profile's folder into the machine's Common Favorites
for k in 'User Shell Folders' 'Shell Folders'; do
    for view in 64 32; do
        timeout -s KILL 120 "$WINE" reg add "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\$k" \
            /v "Common Favorites" /d 'C:\users\sgsystem\Favorites' /reg:$view /f >/dev/null 2>&1
    done
done
RC=0
for p in p64 p32; do
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
