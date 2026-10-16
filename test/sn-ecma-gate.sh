#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# 5906 (CORE review: security): StrongNameSignatureVerification trusts an ECMA-neutral-key assembly
# only inside a Wine Mono installation (Wine's data directories, the prefix's C:\windows\mono), by
# location; a copy of the same framework assembly anywhere else is refused, also when its path goes
# through C:\windows\mono\.. or names a look-alike directory. 64/32 bit.
#   WINE=/opt/wine-sg/bin/wine test/sn-ecma-gate.sh      (ARCHS="x86_64" for one architecture)
# Mutants (mscoree): SG_MUTANT_SN_ECMA_ANY (the key alone passes), SG_MUTANT_SN_ANYWHERE (every
# location trusted): both fail this gate.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ARCHS="${ARCHS:-x86_64 i686}"
for a in $ARCHS; do
    command -v $a-w64-mingw32-gcc >/dev/null || { echo "SKIP: $a-w64-mingw32-gcc not installed"; exit 77; }
done
SYS=""
for d in /opt/wine-sg/share/wine/mono /usr/share/wine/mono /opt/wine/mono; do
    f=$(ls "$d"/wine-mono-*/lib/mono/4.5/System.dll 2>/dev/null | head -1)
    [ -n "$f" ] && { SYS=$f; break; }
done
[ -n "$SYS" ] || { echo "SKIP: no Wine Mono installation with lib/mono/4.5/System.dll"; exit 77; }
RC=0
T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-snecma.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in $ARCHS; do
    TMPDIR="$T" $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/sn-ecma-probe.c" \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/forged" "$C/windows/mono/lib" "$C/windows/monoevil" "$C/users/Public/mono"
cp "$SYS" "$C/forged/System.dll"; cp "$SYS" "$C/windows/mono/lib/System.dll"
cp "$SYS" "$C/windows/monoevil/System.dll"; cp "$SYS" "$C/users/Public/mono/System.dll"
SYSDOS="Z:$(printf '%s' "$SYS" | tr / '\\')"
for a in $ARCHS; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" \
        1 "$SYSDOS" \
        1 'C:\windows\mono\lib\System.dll' \
        0 'C:\forged\System.dll' \
        0 'C:\windows\mono\..\..\forged\System.dll' \
        0 'C:\windows\monoevil\System.dll' \
        0 'C:\users\Public\mono\System.dll' \
        2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
