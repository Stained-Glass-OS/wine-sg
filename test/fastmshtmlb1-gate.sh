#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml batch b1: comment/SVG element members, applyElement, rect overrides, designMode, class info, OLE object members.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmlb1-gate.sh   (mutants SG_MUTANT_MSHTML_B1_COMMENT/APPLY/RECT/SVGHTML/DM/SITE)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmlb1.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmlb1-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
case "$(v PCI)" in 00000000*) pci_ok=1;; *) pci_ok=0;; esac; [ "$pci_ok" = 1 ] && [ "$(v PCIBAD)" = "80070057" ] && pass "element IProvideClassInfo2: default source IID, bad kind rejected" || fail "pci: $(v PCI) $(v PCIBAD)"
[ "$(v EV)" = "00000000" ] && pass "onmstransitionend get/put" || fail "ev: $(v EV)"
[ "$(v RECT)" = "00000000 77" ] && [ "$(v CFP)" = "00000000 outside" ] && pass "IHTMLRect put_left sticks; componentFromPoint outside" || fail "rect: $(v RECT) / $(v CFP)"
[ "$(v SVGIN)" = "00000000 1" ] && [ "$(v SVGSET)" = "00000000 1" ] && pass "SVG element innerHTML get and set" || fail "svg innerHTML: $(v SVGIN) / $(v SVGSET)"
[ "$(v SVGLANG2)" = "00000000 de" ] && [ "$(v SVGOFF)" = "00000000" ] && pass "SVG element lang round trip, offsetWidth" || fail "svg lang: $(v SVGLANG2) $(v SVGOFF)"
[ "$(v UPTODATE)" = "00000000" ] && [ "$(v UPDATE)" = "00000000" ] && [ "$(v BADVERB)" = "80040181" ] && pass "IOleObject IsUpToDate/Update, invalid verb" || fail "oleobj: $(v UPTODATE) $(v UPDATE) $(v BADVERB)"
[ "$(v SITE0)" = "80004005" ] && [ "$(v SITE1)" = "00000000 1" ] && pass "IObjectWithSite keeps the site" || fail "site: $(v SITE0) / $(v SITE1)"
[ "$(v DOCMISC)" = "00000000 1" ] && [ "$(v ENUMCLONE)" = "00000000 00000001 00000000" ] && [ "$(v LOCK)" = "00000000" ] && pass "IOleDocument misc status, empty IEnumUnknown clone/skip/reset, LockContainer" || fail "ole misc: $(v DOCMISC) / $(v ENUMCLONE) / $(v LOCK)"
[ "$(v DM0)" = "Inherit" ] && [ "$(v DM1)" = "Off" ] && pass "designMode reads Inherit then Off" || fail "designMode: $(v DM0) $(v DM1)"
[ "$(v CM)" = "k fr 3" ] && [ "$(v CMC)" = "kls" ] && [ "$(v CML)" = "[]" ] && pass "comment element className/lang/tabIndex" || fail "comment: $(v CM) $(v CMC) $(v CML)"
[ "$(v APPIN)" = "<b><i>q</i></b>" ] && [ "$(v APPOUT)" = "U c" ] && pass "applyElement inside and outside" || fail "apply: $(v APPIN) / $(v APPOUT)"
[ "$(v PROTO)" = "true" ] && pass "document.protocol is a name" || fail "protocol: $(v PROTO)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
