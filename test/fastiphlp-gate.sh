#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# iphlpapi's lease/interface-state/forwarding/TTL/TCP/proxy-ARP calls (patch 3179),
# 64- and 32-bit, test/fastiphlp-probe.c against a stand-in sg-netctl (a shell
# script that logs its arguments; never the host's network). Administrators'
# calls become the requests below; a non-administrator gets ERROR_ACCESS_DENIED
# before sg-netctl is asked; sg-netctl's refusals map to Windows errors.
#
#   WINE=/opt/wine-sg/bin/wine test/fastiphlp-gate.sh
# Mutants: SG_MUTANT_NW_NOADMIN, SG_MUTANT_IPH_NOCOUNT (iphlpapi/netwrite.c)
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
T=$(mktemp -d /var/tmp/sg-fastiphlp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/fastiphlp-probe.c" -liphlpapi -lws2_32 -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
cat > "$T/netctl" <<'EOS'
#!/bin/sh
echo "$*" >> "$SG_NETCTL_LOG"
mode=$(cat "$SG_NETCTL_MODE" 2>/dev/null)
if [ -z "$mode" ]; then echo OK; else echo "ERROR $mode stand-in says $mode"; exit 1; fi
EOS
chmod +x "$T/netctl"
export SG_NETCTL="$T/netctl" SG_NETCTL_LOG="$T/log" SG_NETCTL_MODE="$T/mode"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

ADMIN_LOG='release lo
renew lo
disable lo
enable lo
forwarding on
forwarding off
forwarding on
ttl 77
ttl 64
tcp delete 127.0.0.1 PORT 127.0.0.1 PORT
proxyarp add lo 10.9.8.20 255.255.255.0
proxyarp del lo 10.9.8.20 255.255.255.0'
REPLY_LOG='ttl 64'
# row: name | mode file content | netctl path override | probe args | expected log
run() {
    name=$1; mode=$2; netctl=$3; wantlog=$4; shift 4
    for a in x86_64 i686; do
        printf '%s' "$mode" > "$T/mode"; : > "$T/log"
        out=$(cd "$T" && SG_NETCTL="$netctl" timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" "$@" 2>/dev/null </dev/null | tr -d '\r')
        got=$(sed 's/^tcp delete 127.0.0.1 [0-9]* 127.0.0.1 [0-9]*$/tcp delete 127.0.0.1 PORT 127.0.0.1 PORT/' "$T/log")
        if printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && [ "$got" = "$wantlog" ]; then
            echo "PASS  $name ($a)"
        else
            echo "FAIL  $name ($a)"
            printf '%s\n' "$out" | grep -v '^PASS' | sed 's/^/        /'
            if [ "$got" != "$wantlog" ]; then echo "        log differs:"; printf '%s\n' "$got" | diff - "$T/want" | sed 's/^/        /'; fi
            RC=1
        fi
        "$WINESERVER" -w
    done
}
runw() { printf '%s\n' "$4" > "$T/want"; run "$@"; }
runw "an administrator's calls become sg-netctl requests" "" "$T/netctl" "$ADMIN_LOG" admin
runw "a non-administrator is refused before sg-netctl is asked" "" "$T/netctl" "" nonadmin
runw "sg-netctl refuses: ERROR_ACCESS_DENIED" "denied" "$T/netctl" "$REPLY_LOG" reply 5
runw "sg-netctl says invalid: ERROR_INVALID_PARAMETER" "invalid" "$T/netctl" "$REPLY_LOG" reply 87
runw "sg-netctl says failed: ERROR_GEN_FAILURE" "failed" "$T/netctl" "$REPLY_LOG" reply 31
runw "sg-netctl does not know the request: ERROR_NOT_SUPPORTED" "unsupported" "$T/netctl" "$REPLY_LOG" reply 50
runw "no sg-netctl at all: ERROR_NOT_SUPPORTED" "" "/nonexistent/sg-netctl" "" reply 50
runw "ParseNetworkString networks, no-scope forms, named addresses" "" "$T/netctl" "" parse
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
