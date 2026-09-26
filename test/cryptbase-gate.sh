#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# cryptbase.dll (patches/sg/0303): Windows' base cryptography DLL, forwarding
# its SystemFunction exports to advapi32. Mozilla's mozglue.dll imports
# CRYPTBASE.SystemFunction036 (RtlGenRandom) since Firefox 140: Zotero 10
# failed to start with c0000135 before it. Loaded by the name mozglue uses,
# from a 64-bit and a 32-bit program.
#
#   WINE=/opt/wine-sg/bin/wine test/cryptbase-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-cryptbase.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" \
    XDG_DESKTOP_DIR="$T/home/Desktop"
mkdir -p "$HOME/Desktop"
export WINEDLLOVERRIDES="winemenubuilder.exe=d${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"
x86_64-w64-mingw32-gcc -O2 -o "$T/cb64.exe" "$HERE/cryptbase-probe.c" || { echo "FAIL  probe"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/cb32.exe" "$HERE/cryptbase-probe.c" || { echo "FAIL  probe"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all
"$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w

for b in 64 32; do
    r=$("$WINE" "$T/cb$b.exe" 2>/dev/null | tr -d '\r')
    case "$r" in *"loaded=1 random=1"*) pass "$b-bit: CRYPTBASE.dll loads and SystemFunction036 fills a buffer ($r)" ;;
        *) fail "$b-bit: '$r'" ;; esac
done
[ -f "$WINEPREFIX/drive_c/windows/system32/cryptbase.dll" ] && pass "a new prefix has system32\\cryptbase.dll" ||
    fail "no system32\\cryptbase.dll in a new prefix"

[ $RC = 0 ] && echo "cryptbase-gate: all passed" || echo "cryptbase-gate: FAILURES"
exit $RC
