#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: document.createProcessingInstruction (PI node object) and document.adoptNode across documents.
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmladopt-gate.sh   (mutants SG_MUTANT_MSHTML_NOPI, SG_MUTANT_MSHTML_NOADOPT)
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
T=$(mktemp -d /var/tmp/sg-fastmshtmladopt.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmladopt-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
chk() { [ "$(v "$1")" = "$2" ] && pass "$3" || fail "$3: got '$(v "$1")'"; }
chk PI0 "7;xml-stylesheet;href='a.css';xml-stylesheet;href='a.css';object" "createProcessingInstruction: node type, target, data, name, value, constructor"
chk PI1 "two;two;c;1;t" "data setter, appendChild, cloneNode keep the target"
chk PI2 "error" "an invalid target is refused"
chk AD0 "true;true;true;true;true;true;s1;t1" "adoptNode: node, wrapped descendants and attributes move to the new document, source tree loses it"
chk AD1 "true;true;true;true" "adopted subtree can be appended and found in the new document"
chk AD2 "true;true;true" "adoptNode within one document detaches the node"
chk AD3 "true;hi" "adoptNode of a text node"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
