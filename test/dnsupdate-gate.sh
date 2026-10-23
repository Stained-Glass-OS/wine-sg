#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dnsapi dynamic update, RFC 2136 (6070, N-DNS): DnsModifyRecordsInSet_* and DnsReplaceRecordSet* said success
# and sent nothing. The gate runs Wine in a network and mount namespace of its own (unshare -rnm: nothing on the
# real network, the real /etc/resolv.conf untouched) with its own name server for sg.test and locked.test:
#  - an A record is added, a second one, one is swapped for another (delete then add), the set replaced, the
#    last deleted -- each checked with DnsQuery against the server; an SRV record; the _A form;
#  - records of different names, no records: ERROR_INVALID_PARAMETER; a zone that refuses updates:
#    DNS_ERROR_RCODE_REFUSED; a name in no zone: DNS_ERROR_RCODE_NAME_ERROR; a secure-only update (GSS-TSIG,
#    not available): ERROR_NOT_SUPPORTED, nothing sent.
#   WINE=/opt/wine-sg/bin/wine test/dnsupdate-gate.sh   (mutant SG_MUTANT_UPDATE_NO_DELETE)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" python3 unshare ip; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -rnm true 2>/dev/null || { echo "SKIP: no unprivileged user and network namespaces"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dnsupdate.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/dnsupdate-probe.exe" "$HERE/dnsupdate-probe.c" -ldnsapi -lws2_32 || { fail "the probe did not build"; exit 1; }
echo "nameserver 127.0.0.1" > "$T/resolv.conf"
cat > "$T/inner.sh" <<INNER
set -u
ip link set lo up
mount --bind "$T/resolv.conf" /etc/resolv.conf
python3 "$HERE/dnsupdate-server.py" "$T/updates.log" > "$T/server.out" 2>&1 &
SRV=\$!
i=0; while ! grep -q ready "$T/server.out" 2>/dev/null && [ \$i -lt 50 ]; do sleep 0.1; i=\$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dnsupdate-probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" dnsupdate-probe.exe > "$T/out" 2>/dev/null </dev/null)
"$WINESERVER" -k 2>/dev/null
kill \$SRV
INNER
unshare -rnm sh "$T/inner.sh"
out=$(tr -d '\r' <"$T/out" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect Add "0 10.1.2.3" "DnsModifyRecordsInSet_W adds an A record"
expect AddSecond "0 10.1.2.3,10.1.2.4" "and a second"
expect Modify "0 10.1.2.4,10.1.2.5" "deleting one and adding another in one call"
expect Replace "0 10.9.9.9" "DnsReplaceRecordSetW replaces the set"
expect Delete "0 none" "deleting the last record"
expect AddSrv "0 8443 1 5 host.sg.test" "an SRV record"
expect AddAnsi "0 10.7.7.7" "DnsModifyRecordsInSet_A"
expect MixedNames 87 "records of different names: ERROR_INVALID_PARAMETER"
expect Nothing 87 "no records: ERROR_INVALID_PARAMETER"
expect Refused 9005 "a zone that refuses updates: DNS_ERROR_RCODE_REFUSED"
expect NoZone 9003 "a name in no zone: DNS_ERROR_RCODE_NAME_ERROR"
expect SecureOnly 50 "a secure-only update: ERROR_NOT_SUPPORTED"
expect done 1 "the probe ran to the end"
n=$(wc -l < "$T/updates.log" 2>/dev/null || echo 0)
[ "$n" = 7 ] && pass "the server took exactly the seven updates (none for the refused, failed and secure-only calls)" \
    || fail "the server took $n updates, wanted 7"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
