#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dnsapi DNS-SD (6068, N-DNS): DnsServiceBrowse was a stub that returned success and never called back;
# DnsServiceResolve and the instance helpers were missing. They now go through the system's avahi daemon:
#  - the gate publishes a uniquely named test service with avahi-publish for the run (an avahi client that
#    advertises only while it runs; no avahi or firewall configuration is changed);
#  - DnsServiceBrowse (V1 and V2 callbacks) finds it: its PTR, then SRV (the port), TXT and address records;
#    bad arguments ERROR_INVALID_PARAMETER; returns DNS_REQUEST_PENDING;
#  - DnsServiceResolve gives the instance: its name, host, port, address and TXT properties (a bare key has an
#    empty value); an instance nobody publishes fails in the callback; a name without an instance is refused;
#  - DnsServiceConstructInstance / CopyInstance / FreeInstance;
#  - DnsStartMulticastQuery finds a record (the instance's SRV, this machine's A) and DnsStopMulticastQuery
#    stops it, no callback coming after;
#  - when the publisher goes away, the browse reports a PTR with no time to live; cancel ends it with
#    ERROR_CANCELLED, once;
#  - DnsServiceRegister advertises through the avahi daemon (avahi-browse sees it, with its port and TXT) only
#    while the program holds it: DnsServiceDeRegister withdraws it (with its callback), and so does the
#    program's end; a second registration of the name is refused.
#   WINE=/opt/wine-sg/bin/wine test/dnssd-gate.sh   (mutants SG_MUTANT_MDNS_PTR_ONLY, SG_MUTANT_MDNS_KEEP_REGISTERED,
#   SG_MUTANT_MDNS_STOP_NOWAIT)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" avahi-publish; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
avahi-browse -t -p _sgnone._tcp >/dev/null 2>&1 || { echo "SKIP: the avahi daemon is not running"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dnssd.XXXXXX)
PUB=""
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { [ -n "$PUB" ] && kill "$PUB" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/dnssd-probe.exe" "$HERE/dnssd-probe.c" -ldnsapi || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/dnssd-probe.exe" "$C/"
NAME="SG Probe $$"
TYPE="_sgprobe$$._tcp"
avahi-publish -s "$NAME" "$TYPE" 4242 "k1=v1" "flag" >/dev/null 2>&1 & PUB=$!
sleep 1
(cd "$C" && timeout -s KILL 150 "$WINE" dnssd-probe.exe "$NAME" "$TYPE" >"$T/out" 2>"${DNSSD_LOG:-/dev/null}" </dev/null) &
PROBE=$!
# the probe asks for the publisher to go once it found the service
i=0; while [ ! -e "$C/found.flag" ] && [ $i -lt 120 ] && kill -0 "$PROBE" 2>/dev/null; do sleep 0.5; i=$((i + 1)); done
kill "$PUB" 2>/dev/null; PUB=""
wait "$PROBE"
out=$(tr -d '\r' <"$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect BrowseNull 87 "DnsServiceBrowse without a request: ERROR_INVALID_PARAMETER"
expect BrowseBadName 87 "a name that is no service type: ERROR_INVALID_PARAMETER"
expect Browse 9506 "DnsServiceBrowse: DNS_REQUEST_PENDING"
expect Found 1 "the published service is found"
expect Records "4242 1 1" "with its SRV (port), TXT and address records"
expect BrowseV2 "9506 1" "the V2 callback too"
expect CancelV2 0 "DnsServiceBrowseCancel"
expect Resolve "9506 0 1" "DnsServiceResolve answers"
expect Instance "1 4242 1 1 1" "the instance: name, port, host, address, TXT properties"
expect ResolveMissing "9506 1 1" "an instance nobody publishes fails in the callback"
expect ResolveNoInstance 87 "a name without an instance is refused"
expect Construct "1 1 80 2 1" "DnsServiceConstructInstance and DnsServiceCopyInstance"
expect QuerySrv "0 1 1 1" "DnsStartMulticastQuery: the instance's SRV record, the handle filled in"
expect QueryStop "0 1" "DnsStopMulticastQuery returns once the running callback is done"
expect QueryQuiet 1 "no callback after it returns"
expect QueryA "0 1 1" "this machine's address record by its .local name"
expect QueryStopAgain 87 "a stopped query cannot be stopped again"
expect QueryNull 87 "no request: ERROR_INVALID_PARAMETER"
expect Removed 1 "the publisher gone, the browse reports a PTR with no time to live"
expect Cancel "0 1 1223" "cancelling the browse: ERROR_CANCELLED in the callback"
expect CancelAgain 1223 "a cancelled browse cannot be cancelled again"
expect done 1 "the probe ran to the end"

# registering
RNAME="SG Reg $$"
RTYPE="_sgreg$$._tcp"
seen() { timeout 20 avahi-browse -t -r "$RTYPE" 2>/dev/null | grep '^=' | grep -c "SG Reg $$"; }
(cd "$C" && timeout -s KILL 150 "$WINE" dnssd-probe.exe register "$RNAME" "$RTYPE" >"$T/rout" 2>/dev/null </dev/null) &
PROBE=$!
waitflag() { i=0; while [ ! -e "$C/$1" ] && [ $i -lt 120 ] && kill -0 "$PROBE" 2>/dev/null; do sleep 0.5; i=$((i + 1)); done; }
waitflag registered.flag
r1=$(timeout 20 avahi-browse -t -r "$RTYPE" 2>/dev/null)
touch "$C/checked.flag"
waitflag deregistered.flag
sleep 2
r2=$(seen)
touch "$C/checked2.flag"
waitflag registered2.flag
r3=$(seen)
touch "$C/checked3.flag"
wait "$PROBE"
sleep 2
r4=$(seen)
out=$(tr -d '\r' <"$T/rout")
printf '%s\n' "$out" | sed 's/^/      /'
expect RegisterNull 87 "DnsServiceRegister without a request: ERROR_INVALID_PARAMETER"
expect Register "9506 1 0 1" "DnsServiceRegister: DNS_REQUEST_PENDING, then the callback with the instance"
expect RegisterAgain 9711 "a second registration of the name: DNS_ERROR_RECORD_ALREADY_EXISTS"
if printf '%s\n' "$r1" | grep -q "SG Reg $$" && printf '%s\n' "$r1" | grep -q 'port = \[4343\]' &&
   printf '%s\n' "$r1" | grep -q '"sg=yes"'; then pass "the avahi daemon advertises it, port and TXT"
else fail "avahi-browse did not see the registration: $(printf '%s' "$r1" | tr '\n' '|' | cut -c1-300)"; fi
expect DeRegister "9506 1 0 1" "DnsServiceDeRegister: pending, then its callback"
[ "$r2" = 0 ] && pass "after DnsServiceDeRegister it is no longer advertised" || fail "still advertised after DnsServiceDeRegister ($r2)"
expect DeRegisterAgain 1168 "deregistering what is not registered: ERROR_NOT_FOUND"
expect RegisterExit "9506 1 0" "registered again"
[ "$r3" -ge 1 ] 2>/dev/null && pass "advertised again" || fail "not advertised again ($r3)"
[ "$r4" = 0 ] && pass "the program's end withdraws it" || fail "still advertised after the program ended ($r4)"
expect done 1 "the register probe ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
