#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A registry key's security by name in the 32- and 64-bit views
# (SE_REGISTRY_WOW64_32KEY, SE_REGISTRY_WOW64_64KEY; patches/sg/0799), as
# for SE_REGISTRY_KEY. They were "not currently supported" (the DACL NULL):
# Opera's installer asks for HKCU\Software\Classes so, read the NULL DACL
# and crashed in GetExplicitEntriesFromAcl -- Opera half installed.
#
#   WINE=/opt/wine-sg/bin/wine test/wow64keysec-gate.sh   (mutant SG_MUTANT_NO_WOW64_KEY_TYPES)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wow64keysec.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/wow64keysec-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
for k in WOW32 WOW64; do
    line=$(sed -n "s/^$k //p" "$T/o")
    case "$line" in "0 dacl entries 0 "*" set 0") pass "$k: its DACL by name, its entries, set back ($line)";;
        *) fail "$k: '$line' (SE_REGISTRY_KEY: '$(sed -n 's/^KEY //p' "$T/o")')";; esac
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
