#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msxml3 stub batch (patches/sg/2008): test/msxmlstub-probe.c exercises
# IXSLTemplate::stylesheet, IXSLProcessor input / ownerTemplate / stylesheet /
# startMode / startModeURI / readyState / reset (and a real transform starting
# in each mode), IXMLDOMNode::specified / parsed on every node type, and the
# document type node's read-only members.
#
#   WINE=/opt/wine-sg/bin/wine test/msxmlstub-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/msxml3): SG_MUTANT_TEMPLATE_STYLESHEET (stylesheet.c),
# START_MODE (node.c), READYSTATE and RESET_KEEP (stylesheet.c),
# SPECIFIED_NULL and PARSED_NULL (the node implementations).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msxmlstub.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/msxmlstub-probe.exe" "$HERE/msxmlstub-probe.c" \
    -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/msxmlstub-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" msxmlstub-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  probe did not finish (crashed?)'
exit 1
