#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# rpcrt4 (6107): Avast's Icarus installer engine (CCleaner 7's) talks between its master and slave
# processes by asynchronous ncalrpc calls with context handles. Its stubs say NDR 6.2 (async calls were
# refused: 1829), an async manager routine got NULL for its handle_t parameter (fault 0x3e6), and a
# context handle made over one connection of an association was unknown over another (0x1c00001a).
# A widl-generated client and server in two processes (test/rpcicarus-probe.c, test/rpcicarus.idl);
# the generated stubs are made to say NDR 6.2.
#   WINE=/path/to/build/wine test/rpcicarus-gate.sh   (WIDL=... when not beside $WINE)
# Mutants: SG_MUTANT_ASYNC_NDR61, SG_MUTANT_ASYNC_NO_HANDLE (ndr_stubless.c), SG_MUTANT_ASSOC_GROUP_LATE (rpc_assoc.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WIDL="${WIDL:-$(dirname "$WINE")/tools/widl/widl}"
[ -x "$WIDL" ] || WIDL="$(dirname "$WINE")/widl"
[ -x "$WIDL" ] || { echo "SKIP: no widl"; exit 77; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-rpcicarus.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
cp "$HERE/rpcicarus.idl" "$T/"
(cd "$T" && "$WIDL" --win64 -Oif --prefix-server=s_ -h -H rpcicarus.h -o /dev/null rpcicarus.idl \
    && "$WIDL" --win64 -Oif --prefix-server=s_ -c -o rpcicarus_c.c rpcicarus.idl \
    && "$WIDL" --win64 -Oif --prefix-server=s_ -s -o rpcicarus_s.c rpcicarus.idl) \
    || { echo "FAIL  widl did not run"; exit 1; }
# the stub descriptions say NDR 6.2, as current MIDL's do
sed -i 's/0x50002, \/\* Ndr library version \*\//0x60002, \/* Ndr library version *\//' "$T/rpcicarus_c.c" "$T/rpcicarus_s.c"
grep -q '0x60002' "$T/rpcicarus_c.c" || { echo "FAIL  the stub version was not found"; exit 1; }
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -I"$T" -o "$T/probe.exe" "$HERE/rpcicarus-probe.c" \
    "$T/rpcicarus_c.c" "$T/rpcicarus_s.c" -lrpcrt4 || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
(cd "$T" && exec timeout -s KILL 120 "$WINE" "$T/probe.exe" server sg_icarus_ep >"$T/server.out" 2>&1 </dev/null) &
SRV=$!
n=0
until grep -q READY "$T/server.out" 2>/dev/null || [ $n -ge 60 ]; do sleep 1; n=$((n + 1)); done
grep -q READY "$T/server.out" || { echo "FAIL  the server did not start"; cat "$T/server.out"; exit 1; }
out=$(cd "$T" && timeout -s KILL 100 "$WINE" "$T/probe.exe" client sg_icarus_ep 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the client gave no result)"
exit 1
