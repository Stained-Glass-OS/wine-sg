#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Certificate store resync and store locations (patches/sg/1662), 64- and
# 32-bit: test/certresync-probe.c watches a store with
# CERT_STORE_CTRL_AUTO_RESYNC and CERT_STORE_CTRL_NOTIFY_CHANGE while another
# handle adds certificates, cancels the notification, installs and starts a
# service that writes its CERT_SYSTEM_STORE_CURRENT_SERVICE store, and opens
# and lists the services, users and enterprise stores (the machine's Root
# holding the enterprise one, CertEnumPhysicalStore). These were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/certresync-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_AUTO_RESYNC (crypt32/regstore.c),
# SG_MUTANT_NO_POLICY_STORES and SG_MUTANT_NO_CURRENT_SERVICE
# (crypt32/store.c).
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
T=$(mktemp -d /var/tmp/sg-certresync.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" -municode "$HERE/certresync-probe.c" -lcrypt32 -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for a in x86_64 i686; do
    echo "== $a"
    out=$(cd "$T" && timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
