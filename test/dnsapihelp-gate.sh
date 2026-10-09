#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dnsapi record type names, DNS question writing, resolver context handles,
# the cache flush calls and the cache table (patches/sg/2407), 64- and 32-bit:
# the probe (test/dnsapihelp-probe.c) checks DnsRecordStringForType,
# DnsRecordStringForWritableType, DnsRecordTypeForName,
# DnsWriteQuestionToBuffer_{UTF8,W}, DnsAcquireContextHandle_*,
# DnsReleaseContextHandle, DnsFlushResolverCache* and DnsGetCacheDataTable,
# which were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/dnsapihelp-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dnsapi/util.c): SG_MUTANT_DNH_TYPES, _WRITABLE, _CASE, _XID,
# _WIRE, _SIZE, _NAME, _CTX, _CACHE, _FLUSH.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-dnsapihelp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/dnsapihelp-probe.c" -luser32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
for a in x86_64 i686; do
    echo "== $a"
    # each run starts from a fresh profile: the probe writes the hosts file
    "$WINESERVER" -k 2>/dev/null; rm -rf "$WINEPREFIX"; mkdir -p "$WINEPREFIX"
    timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
    "$WINESERVER" -w
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
