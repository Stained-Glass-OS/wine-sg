#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SSPI identities (6258): SspiCopyAuthIdentity, SspiCompareAuthIdentities, SspiMarshalAuthIdentity,
# SspiUnmarshalAuthIdentity, SspiValidateAuthIdentity, SspiIsAuthIdentityEncrypted (sspicli, forwarded
# by secur32), which OneDrive's sync session code imports. test/sspiident-probe.c, 32- and 64-bit.
# Mutant: SG_MUTANT_COMPARE_IGNORES_PASSWORD (dlls/sspicli/main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $cc >/dev/null || { echo "SKIP: $cc missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sspiident.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/sspiident-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/sspiident-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for b in 64 32; do
  out=$(timeout -s KILL 60 "$WINE" "$T/p$b.exe" 2>/dev/null </dev/null | tr -d '\r')
  printf '%s\n' "$out" | sed "s/^/  [$b] /"
  printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
