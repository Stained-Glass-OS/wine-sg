#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# UI Automation recheck (patch 3261): IEnumVARIANT::Clone of the bridge selection and the runtime id of an MSAA proxy. Built on the uiabridge probe:
# test/fastuiarc-probe.c owns a window that answers WM_GETOBJECT with
# UiaReturnRawElementProvider (a pane, a button, a check box), and a copy of
# itself reads it through MSAA: AccessibleObjectFromWindow(OBJID_CLIENT),
# names, roles, states, children, navigation, location, hit test, focus,
# default actions and UIA_PFIA_UNWRAP_BRIDGE. OBJID_CLIENT got nothing.
# 64-bit, and 32-bit when i686-w64-mingw32-gcc is there.
#
#   WINE=/opt/wine-sg/bin/wine test/fastuiarc-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_UIA_ENUMCLONE (uia_bridge.c), SG_MUTANT_UIA_RUNTIMEID (uia_provider.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastuiarc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-luiautomationcore -lole32 -loleaut32 -luuid -loleacc -Wl,--allow-multiple-definition"
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastuiarc64.exe" "$HERE/fastuiarc-probe.c" $LIBS \
    || { echo "FAIL  probe did not build"; exit 1; }
ARCHS=64
if command -v "$MINGW32" >/dev/null; then
    TMPDIR=/var/tmp "$MINGW32" -O1 -o "$T/fastuiarc32.exe" "$HERE/fastuiarc-probe.c" $LIBS \
        || { echo "FAIL  32-bit probe did not build"; exit 1; }
    ARCHS="64 32"
fi
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/fastuiarc*.exe "$WINEPREFIX/drive_c/"
rc=0
for a in $ARCHS; do
    cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" fastuiarc$a.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe$a.out"
EOS
    chmod +x "$T/run.sh"
    timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
    echo "== $a-bit"
    cat "$T/probe$a.out"
    grep -qx 'RESULT: PASS' "$T/probe$a.out" || rc=1
done
exit $rc
