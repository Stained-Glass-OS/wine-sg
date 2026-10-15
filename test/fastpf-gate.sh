#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# iphlpapi packet filtering Pf* (the documented Vista-and-later failure) and ICMPv6 echo (patch 3187), 64- and 32-bit,
# test/fastpf-probe.c. The firewall must not be asked for anything: a stand-in
# sg-admind spool directory is watched (never the real firewall).
#
#   WINE=/opt/wine-sg/bin/wine test/fastpf-gate.sh
# Mutants: SG_MUTANT_PF_SUCCEED, SG_MUTANT_PF_NOCHECK, SG_MUTANT_PF_LOG (iphlpapi/pfilter.c)
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
T=$(mktemp -d /var/tmp/sg-fastpf.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export SG_ADMIN_SPOOL="$T/spool"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$SG_ADMIN_SPOOL/requests" "$SG_ADMIN_SPOOL/replies" "$WINEPREFIX"
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastpf-probe.c" -liphlpapi -lws2_32 -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

probe() { # name arch args...
    name=$1; a=$2; shift 2
    out=$(cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe-$a.exe" "$@" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then
        echo "PASS  $name ($a)"
    else
        echo "FAIL  $name ($a)"; printf '%s\n' "$out" | grep -v '^PASS' | sed 's/^/        /'; RC=1
    fi
    LAST=$out
    "$WINESERVER" -w
}
for a in x86_64 i686; do
    rm -f "$SG_ADMIN_SPOOL"/requests/* 2>/dev/null
    probe "every Pf* call fails as on Windows Vista and later, enforcing nothing" $a api
    n=$(ls "$SG_ADMIN_SPOOL"/requests 2>/dev/null | wc -l)
    if [ "$n" = 0 ]; then echo "PASS  the firewall was not asked for anything ($a)"
    else echo "FAIL  the firewall was asked $n times ($a)"; RC=1; fi
    probe "the ICMPv6 echo calls" $a icmp6
    printf '%s\n' "$LAST" | grep SKIP | sed 's/^/        /'
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
