#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml/jscript: DOM exceptions thrown with name, code and number (0x80700000|code).
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmldomex-gate.sh   (mutants SG_MUTANT_JS_NODOMEX, SG_MUTANT_MSHTML_NOENUMMETH)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-fastmshtmldomex.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmldomex-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
v() { sed -n "s/^$1=//p" "$T/o" | head -1; }
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
[ "$(v X1)" = "InvalidStateError:11:8070000b:true:InvalidStateError" ] && pass "responseType before open() throws a DOMException: name, code, number" || fail "responseType: $(v X1)"
[ "$(v X2)" = "nothrow" ] && pass "an unknown responseType is ignored once the request is open" || fail "responseType open: $(v X2)"
[ "$(v S1)" = "NoModificationAllowedError:7:80700007:true:NoModificationAllowedError" ] && pass "writing a currentStyle property is NoModificationAllowedError" || fail "currentStyle: $(v S1)"
case "$(v E1)" in TypeError*) pass "ordinary script errors are not DOMExceptions";; *) fail "script error: $(v E1)";; esac
[ "$(v EN1)" = "true,true,true" ] && [ "$(v EN2)" = "false,false" ] && pass "DOM methods are enumerated by for-in, except those of Storage" || fail "enumeration: $(v EN1) $(v EN2)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
