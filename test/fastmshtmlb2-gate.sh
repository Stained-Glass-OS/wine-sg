#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml batch b2: SVG value objects and members, window opener/offscreenBuffering/maxConnections, DOMImplementation createDocumentType/createDocument, document media/parentDocument/class info.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlb2-gate.sh   (mutants SG_MUTANT_B2_SVGLEN/SVGMAT/OPENER/DOCTYPE/CLASSINFO)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlb2.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlb2-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(v NS)" = "tspan" ] && pass "createElementNS svg tspan builds its dispex info" || fail "createElementNS: $(v NS)"
[ "$(v W)" = "10 1" ] && [ "$(v WS)" = "2cm 76 6" ] && pass "SVGAnimatedLength reads the attribute; valueAsString writes it back (2cm = 76px, unit cm)" || fail "svg length: $(v W) / $(v WS)"
[ "$(v CIR)" = "3 2" ] && pass "circle cx/r animated lengths" || fail "circle: $(v CIR)"
[ "$(v MAT)" = "7,9 0.5" ] && pass "SVGMatrix translate.scale, SVGPoint.matrixTransform, inverse" || fail "matrix: $(v MAT)"
[ "$(v ANG)" = "3.142 3.14159rad" ] && pass "SVGAngle convertToSpecifiedUnits (180deg in rad)" || fail "angle: $(v ANG)"
[ "$(v TR)" = "4 90 1" ] && pass "SVGTransform.setRotate" || fail "transform: $(v TR)"
[ "$(v OWN)" = "1 sv 1" ] && [ "$(v OWNTOP)" = "00000000 1" ] && pass "ISVGElement ownerSVGElement/viewportElement (outermost svg: null)" || fail "owner: $(v OWN) / $(v OWNTOP)"
[ "$(v GEBI)" = "ci null" ] && pass "SVGSVGElement.getElementById stays inside the svg" || fail "getElementById: $(v GEBI)"
[ "$(v SCALE)" = "2 true 0.265" ] && pass "currentScale, pauseAnimations, pixelUnitToMillimeterX" || fail "scale: $(v SCALE)"
[ "$(v XB)" = "http://x/" ] && pass "SVG xmlbase round trip" || fail "xmlbase: $(v XB)"
[ "$(v TXT)" = "3 true true 1" ] && pass "SVG text: character count, layout lengths, lengthAdjust" || fail "text: $(v TXT)"
[ "$(v OPEN0)" = "undefined" ] && [ "$(v OPEN)" = "true" ] && pass "window.opener round trip" || fail "opener: $(v OPEN0) $(v OPEN)"
[ "$(v OFF0)" = "auto" ] && [ "$(v OFF)" = "true" ] && pass "window.offscreenBuffering default auto, then set" || fail "offscreen: $(v OFF0) $(v OFF)"
[ "$(v MCS)" = "true" ] && pass "window.maxConnectionsPerServer" || fail "maxconn: $(v MCS)"
[ "$(v DT)" = "x:y" ] && pass "createDocumentType" || fail "doctype: $(v DT)"
[ "$(v CD)" = "HTML" ] && pass "createDocument" || fail "createDocument: $(v CD)"
[ "$(v MEDIA0)" = "00000000 screen" ] && [ "$(v MEDIA1)" = "00000000 print" ] && pass "document media default and set" || fail "media: $(v MEDIA0) $(v MEDIA1)"
[ "$(v PARENT)" = "00000000 1" ] && pass "top level parentDocument is NULL, S_OK" || fail "parent: $(v PARENT)"
[ "$(v CI2)" = "00000000 1 80070057" ] && [ "$(v MCI)" = "00000000 1" ] && pass "document IProvideClassInfo2 default source IID, bad kind rejected" || fail "classinfo: $(v CI2) $(v MCI)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
