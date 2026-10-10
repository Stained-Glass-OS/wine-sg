#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# qdvd's DVD navigator and DVD graph builder without a disc (patches/sg/2900):
# test/qdvd-nav-probe.c calls every IDvdControl2 / IDvdInfo2 method of the navigator
# in its only state (stop domain, no disc), the state that can be set while stopped
# (parental level and country, default languages, options, DVD directory), and the
# IDvdGraphBuilder methods, and compares each HRESULT with the documented one. The
# run's log is also checked: none of them may log a FIXME any more.
#
#   WINE=/opt/wine-sg/bin/wine test/qdvd-nav-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (qdvd, -DSG_MUTANT_x): DOMAIN (the stop domain accepts commands),
# ARGS (arguments are not validated), STATE (settings are not stored),
# BUILDER (the render flags are not validated).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-qdvdnav.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all,+err,fixme+quartz WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/qdvd-nav-probe.exe" "$HERE/qdvd-nav-probe.c" -lstrmiids -luuid -lole32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/qdvd-nav-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" qdvd-nav-probe.exe 2>"$T/stderr.log" </dev/null | sed -u 's/\r\$//' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
if grep -E 'fixme:quartz:(dvd_control2|dvd_info2|graph_builder)_' "$T/stderr.log"; then
    echo "FAIL  a formerly stubbed function logged a FIXME"
    exit 1
fi
echo "PASS  none of the formerly stubbed functions logged a FIXME"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
