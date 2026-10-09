#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CNG algorithm names of built-in OIDs (patches/sg/1532): Windows gives the
# hash, encryption, public key and signature OIDs a pwszCNGAlgid (and a
# signature its public key's in pwszCNGExtraAlgid). Wine had NULL; Access
# read the hash's as it checked a database's signature and crashed opening
# any existing database.
#
#   WINE=/opt/wine-sg/bin/wine test/oidcng-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-oidcng.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/oidcng-probe.c" -lcrypt32 || { fail "probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/oidcng-probe.c" -lcrypt32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in 64 32; do
    O=$(timeout 60 "$WINE" "$T/p$a.exe" 2>/dev/null | tr -d '\r' | tr '\n' ' ')
    echo "      $a: $O"
    case "$O" in "2.16.840.1.101.3.4.2.1 SHA256| 1.3.14.3.2.26 SHA1| 1.2.840.113549.2.5 MD5| "*) pass "$a-bit: hash OIDs name their CNG algorithm" ;; *) fail "$a-bit hash: $O" ;; esac
    case "$O" in *"1.2.840.113549.1.1.11 SHA256|RSA 1.2.840.113549.1.1.1 RSA| 1.2.840.113549.3.7 3DES| ") pass "$a-bit: signature, public key and cipher OIDs too" ;; *) fail "$a-bit others: $O" ;; esac
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
