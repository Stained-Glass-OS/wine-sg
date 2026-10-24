#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wsdapi discovery provider (6075, N-WEBSERVICES/WS-Discovery): WSDCreateDiscoveryProvider returned E_NOTIMPL, so
# programs that look for WS-Discovery devices (printers, scanners, cameras, media servers) found none. The gate
# runs in a network namespace of its own (unshare -rnm: only a loopback interface, multicast on), with
# test/wsdprovider-device.py as a target service:
#  - argument checks: no out pointer E_POINTER; a search before Attach E_ABORT; Attach(NULL) E_POINTER; a second
#    Attach E_ABORT; a bad address family E_INVALIDARG, one after Attach STG_E_INVALIDFUNCTION; no id, a tag
#    past WSD_MAX_TEXT_LENGTH, an address that is no http(s) URL, a MatchBy without scopes: E_INVALIDARG;
#  - SearchByType sends a multicast Probe and reports the ProbeMatch (endpoint, both types with their namespace,
#    scopes, XAddrs, metadata version, instance id, remote address, the search's tag), then SearchComplete after
#    the match timeout (ten seconds); a reply relating to another message is not taken; types nobody has:
#    SearchFailed(S_FALSE) with no Add; scopes by RFC 2396 prefix and by strcmp0 (a partial scope fails);
#  - SearchById sends a Resolve and completes at the first ResolveMatch; an unknown id fails;
#  - SearchByAddress posts a directed Probe over HTTP and reports its ProbeMatch; a 404 fails;
#  - Hello and Bye announcements reach Add and Remove while attached, also from Wine's own publisher, which
#    answers a search as well: its listener was bound to the adapter's unicast address, which on Linux never
#    receives the group's datagrams, so it heard no probe at all; it binds the wildcard address now. A Probe
#    without Types (they are optional) was dropped before the publisher's handler; it reaches it now;
#  - fixed on the way: a Types list of two or more names crashed the reader (the list's tail was never kept),
#    and the types/URI list writers checked their bounds in bytes against characters (a heap overflow);
#  - the announcement listener (UDP 3702) exists only while the program holds an attachment: after Detach, and
#    after a provider is released while attached, nothing of the program listens there (ss), and no Add comes;
#    a search after Detach is E_ABORT; attaching again works.
#   WINE=/opt/wine-sg/bin/wine test/wsdprovider-gate.sh   (mutants SG_MUTANT_WSD_ANY_RELATES,
#   SG_MUTANT_WSD_NO_FAILED, SG_MUTANT_WSD_KEEP_LISTENING)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" python3 unshare ip ss; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -rnm true 2>/dev/null || { echo "SKIP: no unprivileged user and network namespaces"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wsdprov.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX" "$T/flags"
"$MINGW" -O2 -o "$T/wsdprovider-probe.exe" "$HERE/wsdprovider-probe.c" -lwsdapi -lws2_32 -lole32 \
    || { fail "the probe did not build"; exit 1; }
FLAGDIR=$(printf 'Z:%s' "$T/flags" | tr / '\\')
cat > "$T/inner.sh" <<INNER
set -u
cd "$T"
ip link set lo up
ip link set lo multicast on
ip route add 239.0.0.0/8 dev lo
python3 "$HERE/wsdprovider-device.py" "$T/device.log" 3800 3799 > "$T/device.out" 2>&1 &
DEV=\$!
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wsdprovider-probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 180 "$WINE" wsdprovider-probe.exe 3800 3799 '$FLAGDIR' > "$T/out" 2>"\${WSD_LOG:-/dev/null}" </dev/null) &
PROBE=\$!
for phase in attached detached; do
    i=0; while [ ! -e "$T/flags/\$phase.flag" ] && [ \$i -lt 1200 ] && kill -0 \$PROBE 2>/dev/null; do sleep 0.1; i=\$((i + 1)); done
    sleep 0.6
    ss -uanpH 'sport = :3702' | grep -v python > "$T/\$phase.ss"
    : > "$T/flags/\$phase.ack"
done
wait \$PROBE
"$WINESERVER" -k 2>/dev/null
kill \$DEV
INNER
unshare -rnm sh "$T/inner.sh"
out=$(tr -d '\r' <"$T/out" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
DEV="urn:uuid:5e1d2c3b-0000-4000-8000-00000000c0de"
MATCH="$DEV 2 Printer 1 2 http://127.0.0.1:3800/dev 7 1000 1"
expect CreateNull 0x80004003 "WSDCreateDiscoveryProvider without an out pointer: E_POINTER"
expect Create "0x00000000 1" "WSDCreateDiscoveryProvider"
expect Context "0x00000000 1" "GetXMLContext"
expect SearchUnattached 0x80004004 "a search before Attach: E_ABORT"
expect AttachNull 0x80004003 "Attach(NULL): E_POINTER"
expect FamilyBad 0x80070057 "a bad address family: E_INVALIDARG"
expect Family 0x00000000 "SetAddressFamily(IPv4)"
expect Attach "0x00000000 0x80004004" "Attach, and a second Attach: E_ABORT"
expect FamilyAttached 0x80030001 "SetAddressFamily after Attach: STG_E_INVALIDFUNCTION"
expect BadArgs "0x80070057 0x80070057 0x80070057 0x80070057" "no id, a tag too long, no http address, MatchBy without scopes: E_INVALIDARG"
expect SearchType 0x00000000 "SearchByType starts"
expect SearchNoMatch 0x00000000 "a search for types nobody has starts"
expect SearchScopes "0x00000000 0x00000000 0x00000000" "searches by scope start"
expect SearchId "0x00000000 0x00000000" "SearchById starts"
expect SearchAddress "0x00000000 0x00000000" "SearchByAddress starts"
expect ResolveFast 1 "a resolve ends at its match, before the match timeout"
expect ResolveAdd "$MATCH" "the ResolveMatch: endpoint, types (namespace), scopes, XAddrs, metadata, instance, remote"
expect ResolveEnd "1 1" "SearchComplete after one Add"
expect DirectedFast 1 "the directed probe ends with its HTTP reply"
expect DirectedAdd "$MATCH" "the directed probe's ProbeMatch"
expect DirectedEnd "1 1" "SearchComplete after one Add"
expect Directed404 "1 0x00000001 0" "a directed probe answered 404: SearchFailed(S_FALSE), no Add"
expect TypeTime 1 "the multicast search runs for the match timeout (about ten seconds)"
expect TypeAdd "$MATCH" "the ProbeMatch of the type search, with its tag"
expect TypeEnd "1 1 0" "SearchComplete after exactly one Add (the repeated probe is not reported twice)"
expect Decoy 0 "a ProbeMatches relating to another message is not taken"
expect NoMatch "1 0x00000001 0 0" "types nobody has: SearchFailed(S_FALSE), no Add, no SearchComplete"
expect Scopes "1 1 1" "scopes: an RFC 2396 prefix matches, strcmp0 needs the whole scope"
expect ResolveNone "1 0x00000001" "an id nobody has: SearchFailed(S_FALSE)"
expect Hello "$DEV 2 Printer 1 2 http://127.0.0.1:3800/dev 7 1001 1" "a Hello reaches Add (untagged, its own instance id)"
expect Bye "1 0" "a Bye reaches Remove"
expect WineHello "1 Camera" "Wine's publisher's Hello is heard"
expect WineSearch "0x00000000 1 1" "and Wine's publisher answers a search"
expect WineUntyped 1 "a Probe without Types (scopes only) reaches the publisher's handler"
expect WineBye 1 "and its Bye"
expect Detach "0x00000000 0x80004004" "Detach, and a second Detach: E_ABORT"
expect QuietAfterDetach 1 "no Add after Detach"
expect SearchDetached 0x80004004 "a search after Detach: E_ABORT"
expect Attach2 0x00000000 "a second provider attaches (and is released while attached)"
expect Reattach "0x00000000 0x00000000 1" "attaching again works"
expect done 1 "the probe ran to the end"
n=$(wc -l < "$T/attached.ss" 2>/dev/null || echo 0)
[ "$n" -ge 1 ] && pass "while attached, the program listens on UDP 3702" \
    || fail "while attached, nothing of the program listened on UDP 3702"
n=$(wc -l < "$T/detached.ss" 2>/dev/null || echo 9)
[ "$n" = 0 ] && pass "after Detach (and a release while attached), the program listens on nothing" \
    || { fail "after Detach, the program still listens on UDP 3702"; sed 's/^/      /' "$T/detached.ss"; }
grep -q "^probe types=[^ ]*:Printer scopes=None" "$T/device.log" \
    && pass "the device saw the multicast Probe for the type" || fail "the device saw no Probe for Printer"
grep -q "matchby=http://schemas.xmlsoap.org/ws/2005/04/discovery/strcmp0" "$T/device.log" \
    && pass "the probe carried its MatchBy" || fail "no MatchBy reached the device"
grep -q "^http /dev application/soap+xml; charset=utf-8 action=Probe to=http://127.0.0.1:3800/dev" "$T/device.log" \
    && pass "the directed probe was an HTTP POST of a Probe to the address" || fail "no directed Probe reached the device"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
