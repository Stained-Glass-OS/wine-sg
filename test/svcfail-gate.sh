#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Service failure actions and the other ChangeServiceConfig2 levels (patches/sg/2222):
# test/svcfail-probe.c is its own test service, started by services.exe, which restarts it
# or runs a command as the failure actions say.  The service process is the same exe, so
# the 64- and 32-bit runs are separate prefixes' worth of service on one wineserver.
# Takes about a minute per architecture.  No other child process.
#
#   WINE=/opt/wine-sg/bin/wine test/svcfail-gate.sh
# Mutants (programs/services): SG_MUTANT_NO_FAILURE_RESTART, SG_MUTANT_NO_FAILURE_RESET,
# SG_MUTANT_NO_NONCRASH_FLAG (services.c), SG_MUTANT_NO_FAILURE_STORE (rpc.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-svcfail.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DISP=:229
Xvfb "$DISP" -screen 0 1024x768x24 >"$T/xvfb.log" 2>&1 &
XPID=$!
cleanup() { pkill -9 -f "$T/probe-" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/svcfail-probe.c" -ladvapi32 -lkernel32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY="$DISP" "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for a in x86_64 i686; do
    o=$([ $a = x86_64 ] && echo i686 || echo x86_64)
    echo "== $a (other process: $o)"
    out=$(cd "$T" && timeout -s KILL 400 env DISPLAY="$DISP" "$WINE" "$T/probe-$a.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    # shutdown: two services that want the controls are running; shut the prefix down
    # the way wineboot does and see what they were told
    rm -f "$T/sgfail_pre.marks" "$T/sgfail_shut.marks"
    (cd "$T" && timeout -s KILL 120 env DISPLAY="$DISP" "$WINE" "$T/probe-$a.exe" --shutdown-setup 2>/dev/null </dev/null | tr -d '\r' | sed 's/^/      /')
    timeout -s KILL 400 env DISPLAY="$DISP" "$WINE" wineboot --kill --shutdown >/dev/null 2>&1 </dev/null
    timeout -s KILL 60 "$WINESERVER" -w 2>/dev/null
    pre=$(tr '\r\n' '  ' <"$T/sgfail_pre.marks" 2>/dev/null)
    shut=$(tr '\r\n' '  ' <"$T/sgfail_shut.marks" 2>/dev/null)
    echo "      marks: pre=[$pre] shut=[$shut]"
    case "$pre" in *"start  pre-begin  pre-done "*|*"start pre-begin pre-done"*) echo "      PASS  a service that wants PRESHUTDOWN got it and had its time to stop";; *) echo "      FAIL  the preshutdown service was told and waited for"; RC=1;; esac
    case "$shut" in *shut-called*) echo "      PASS  a service that wants SHUTDOWN got it";; *) echo "      FAIL  the shutdown service was told"; RC=1;; esac
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
