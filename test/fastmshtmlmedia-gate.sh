#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: anchor.pathname slash for about:/file:, MediaQueryList.media in IE's normalized form.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlmedia-gate.sh   (mutants SG_MUTANT_MSHTML_NOMEDIA, SG_MUTANT_MSHTML_NOSLASH)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlmedia.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlmedia-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v P1)" = /blank ] && [ "$(v P2)" = /path ] && pass "anchor.pathname gets a leading slash for about: and file:" || fail "about/file pathname: $(v P1) $(v P2)"
[ "$(v P3)" = path ] && [ "$(v P4)" = /about ] && [ "$(v P5)" = path ] && pass "other schemes keep the pathname as it is" || fail "other pathnames: $(v P3) $(v P4) $(v P5)"
[ "$(v M1)" = unknown ] && [ "$(v M2)" = unknown ] && pass "an unknown media type is called unknown" || fail "unknown media: $(v M1) $(v M2)"
[ "$(v M3)" = "not all" ] && [ "$(v M6)" = print ] && pass "an invalid query is 'not all'; a known type is kept" || fail "invalid/known media: $(v M3) $(v M6)"
[ "$(v M4)" = "all and (max-width:0px)" ] && [ "$(v M5)" = "screen and (min-width:10px)" ] && pass "a feature-only query gets 'all and' and loses the space after the colon" || fail "normalized media: $(v M4) / $(v M5)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
