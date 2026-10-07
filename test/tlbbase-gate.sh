#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The type library marshaler for an interface whose base interfaces have no
# proxy/stub registered (patches/sg/1301). Edge's elevation service registers
# only IElevatorEdge (IElevatorEdge : IElevator2 : IElevator :
# IElevatorEdgeBase : IUnknown); Wine delegated the inherited methods to the
# base's own proxy/stub, found none, and every call failed with
# REGDB_E_IIDNOTREG ("Failed to create an IRpcStubBuffer ... 0x80040155";
# Edge's App-Bound cookie encryption never reached the service). The
# inherited methods now come from the type library: ISgEdge (tlbbase.idl, no
# methods of its own, the only one registered) marshals across apartments
# and Add/Mul/Neg (two bases up) answer through the proxy.
# Mutant: SG_MUTANT_TLB_UNREGISTERED_BASE (dlls/rpcrt4/ndr_typelib.c).
#
#   WINE=/opt/wine-sg/bin/wine test/tlbbase-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
WIDL="${WIDL:-}"
for w in "$(dirname "$WINE")/widl" "$(dirname "$WINE")/tools/widl/widl" "$(command -v widl 2>/dev/null)"; do
    [ -z "$WIDL" ] && [ -n "$w" ] && [ -x "$w" ] && WIDL=$w
done
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -n "$WIDL" ] || { echo "SKIP: no widl"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tlbbase.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
D=$(dirname "$WINE"); INC=
for i in "$D/../include/wine/windows" "$D/include" "$D/../wine-10.0/include" /usr/share/mingw-w64/include; do
    [ -z "$INC" ] && [ -f "$i/unknwn.idl" ] && INC=$i
done
[ -n "$INC" ] || { echo "SKIP: no unknwn.idl"; exit 77; }
"$WIDL" -I "$INC" -t -o "$T/tlbbase.tlb" "$HERE/tlbbase.idl" 2>"$T/widl.err" || { fail "widl: $(head -3 "$T/widl.err")"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/tlbbase-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/tlbbase.tlb" "$WINEPREFIX/drive_c/tlbbase.tlb"
DISPLAY= timeout -s KILL 120 "$WINE" "$T/probe.exe" 'C:\tlbbase.tlb' 2>/dev/null </dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v base-registered)" = 0 ] && pass "the base interfaces have no proxy/stub registered (as in Edge)" || fail "base registered: $(v base-registered)"
[ "$(v marshal)" = 00000000 ] && pass "ISgEdge marshals (its stub is built from the type library)" || fail "marshal: $(v marshal)"
[ "$(v unmarshal)" = 00000000 ] && [ "$(v proxy)" = 1 ] && pass "...and unmarshals to a proxy in another apartment" || fail "unmarshal: $(v unmarshal) proxy $(v proxy)"
[ "$(v add)" = "00000000 5" ] && pass "a method two bases up answers through the proxy (Add 2+3)" || fail "add: $(v add)"
[ "$(v mul)" = "00000000 20" ] && [ "$(v neg)" = "00000000 -7" ] && pass "...and the nearer base's (Mul 4*5, Neg 7)" || fail "mul: $(v mul) neg: $(v neg)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
