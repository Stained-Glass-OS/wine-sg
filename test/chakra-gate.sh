#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# chakra.dll: Windows' JavaScript hosting API on QuickJS (patches/sg/0508).
# React Native for Windows (Office's sign-in and other panes) runs its
# JavaScript on chakra.dll; Wine had none, and Word ended ("delay-load
# failure", then Office's crash handler). The probe drives the API the way
# React Native's Chakra runtime does: modern JavaScript; strings both ways
# beyond ASCII; native functions called and constructed, with state; the
# script's exceptions to the host (script and compile errors) and the host's
# into the script; calls with 'this'; external objects and their data and
# finalizer; arrays, indexed properties, ArrayBuffer bytes; symbols as
# property ids; getters by defineProperty; serialized bytecode (and the
# source again for bytes that are not ours); promise jobs handed to the host
# as a task; reference counts.
#
#   WINE=/opt/wine-sg/bin/wine test/chakra-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-chakra.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/chakra-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
[ "$(v create)" = "0 0 0" ] && pass "a runtime and a context, made current" || fail "create: $(v create)"
[ "$(v modern)" = "0 124" ] && pass "modern JavaScript (private fields, destructuring, arrows, ?. ??)" || fail "modern: $(v modern)"
[ "$(v string)" = "0 8:HÉLLO 世界" ] && pass "strings both ways, beyond ASCII" || fail "string: $(v string)"
[ "$(v native)" = "0 105:add:function" ] && pass "a named native function, called with its state" || fail "native: $(v native)"
[ "$(v scriptthrow)" = "196609 0 bad thing" ] && pass "a script's exception reaches the host (JsErrorScriptException)" || fail "script throw: $(v scriptthrow)"
[ "$(v compile)" = 196610 ] && pass "a compile error is JsErrorScriptCompile" || fail "compile: $(v compile)"
[ "$(v hostthrow)" = "0 caught from host true" ] && pass "the host's exception is caught by the script" || fail "host throw: $(v hostthrow)"
[ "$(v construct)" = "0 7:true" ] && [ "$(v constructhost)" = "9 1" ] && pass "a native constructor, from script and from the host" || fail "construct: $(v construct) / $(v constructhost)"
[ "$(v call)" = "0 60" ] && pass "the host calls a script function with this and arguments" || fail "call: $(v call)"
[ "$(v external)" = "0000000000001234 5" ] && [ "$(v dispose)" = "0 1" ] && pass "an external object keeps its data; its finalizer runs" || fail "external: $(v external) / $(v dispose)"
[ "$(v array)" = "two 3 8" ] && [ "$(v arraybuffer)" = "0 4 1234" ] && pass "arrays, indexed properties, ArrayBuffer bytes" || fail "arrays: $(v array) / $(v arraybuffer)"
[ "$(v symbol)" = "1 42 greeting" ] && pass "symbols as property ids; a property id's name" || fail "symbol: $(v symbol)"
[ "$(v define)" = "1 1 got 1 1 1" ] && pass "defineProperty with a getter; own names; strict equality; prototype" || fail "define: $(v define)"
[ "$(v serialized)" = "0 1 42 0 42" ] && [ "$(v samehandle)" = 1 ] && pass "serialized bytecode runs (foreign bytes: the source); one handle per object" || fail "serialized: $(v serialized) / $(v samehandle)"
[ "$(v promise)" = "0 1 no yes 5 async" ] && pass "promise jobs wait for the host's task, which runs them" || fail "promise: $(v promise)"
[ "$(v refs)" = "1 0" ] && pass "reference counts" || fail "refs: $(v refs)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
