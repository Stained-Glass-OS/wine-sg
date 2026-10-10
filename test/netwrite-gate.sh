#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# iphlpapi's calls that change the network (patches/sg/2416), 64- and 32-bit:
# test/netwrite-probe.c against a stand-in sg-netctl (a shell script that logs
# its arguments and answers as SG_NETCTL_MODE says). An administrator's calls
# become the requests below, a non-administrator's are refused with
# ERROR_ACCESS_DENIED before sg-netctl is asked, and sg-netctl's refusals map
# to Windows errors; with no sg-netctl, or one that does not know the
# request, the calls say ERROR_NOT_SUPPORTED; with no sg-netctl at all the old
# ones, which always said NO_ERROR, still do.
#
#   WINE=/opt/wine-sg/bin/wine test/netwrite-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NW_NOADMIN (iphlpapi/netwrite.c).
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
T=$(mktemp -d /var/tmp/sg-netwrite.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/netwrite-probe.c" -liphlpapi -lws2_32 -ladvapi32 \
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

ADMIN_LOG='addr add lo 10.9.8.7/24
addr add lo 10.9.8.7/8
addr add lo fd00::5/64
addr del lo 10.9.8.7/24
route add lo 10.20.0.0/16 --gateway 10.9.8.1 --metric 5
route del lo 10.20.0.0/16 --gateway 10.9.8.1
route set lo 10.20.0.0/16 --gateway 10.9.8.1 --metric 7
route add lo 10.30.0.0/16 --gateway 10.9.8.1 --metric 3
route del lo 10.30.0.0/16 --gateway 10.9.8.1
route set lo 10.30.0.0/16 --gateway 10.9.8.1 --metric 3
neigh add lo 10.9.8.50 00:11:22:33:44:55 --permanent
neigh add lo 10.9.8.52 00:11:22:33:44:55
neigh del lo 10.9.8.52
neigh flush lo --family 4
neigh flush
neigh flush lo --family 4
neigh add lo 10.9.8.51 aa:bb:cc:dd:ee:ff --permanent
neigh del lo 10.9.8.51
addr add lo 10.9.8.9/24
addr del lo 10.9.8.9/24'
REPLY_LOG='addr add lo 10.9.8.7/24
route add lo 10.30.0.0/16 --metric 0'

# row: name | mode file content | netctl path override | probe args | expected log
run() {
    name=$1; mode=$2; netctl=$3; wantlog=$4; shift 4
    for a in x86_64 i686; do
        printf '%s' "$mode" > "$T/mode"; : > "$T/log"
        out=$(cd "$T" && SG_NETCTL="$netctl" timeout -s KILL 180 env DISPLAY= "$WINE" "$T/probe-$a.exe" "$@" 2>/dev/null </dev/null | tr -d '\r')
        got=$(cat "$T/log")
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
runw "sg-netctl refuses: ERROR_ACCESS_DENIED" "denied" "$T/netctl" "$REPLY_LOG" reply 5 5
runw "sg-netctl says invalid: ERROR_INVALID_PARAMETER" "invalid" "$T/netctl" "$REPLY_LOG" reply 87 87
runw "sg-netctl says notfound: ERROR_NOT_FOUND" "notfound" "$T/netctl" "$REPLY_LOG" reply 1168 1168
runw "sg-netctl says failed: ERROR_GEN_FAILURE" "failed" "$T/netctl" "$REPLY_LOG" reply 31 31
runw "sg-netctl says exists: ERROR_OBJECT_ALREADY_EXISTS" "exists" "$T/netctl" "$REPLY_LOG" reply 5010 5010
runw "sg-netctl does not know the request: ERROR_NOT_SUPPORTED for both" "unsupported" "$T/netctl" "$REPLY_LOG" reply 50 50
runw "no sg-netctl at all: ERROR_NOT_SUPPORTED, but NO_ERROR for the old calls" "" "/nonexistent/sg-netctl" "" reply 50 0
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
