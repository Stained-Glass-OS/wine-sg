#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# UI Automation event handler groups (patches/sg/2675), on Xvfb:
# test/uiagroup-probe.c builds an IUIAutomationEventHandlerGroup with one
# handler of each kind, registers it with AddEventHandlerGroup, checks each
# handler gets the events another process raises (only the property and text
# edit kind it asked for), and that RemoveEventHandlerGroup stops them all.
# CreateEventHandlerGroup, AddEventHandlerGroup and RemoveEventHandlerGroup
# were E_NOTIMPL stubs. 64-bit, and 32-bit when i686-w64-mingw32-gcc is there.
#
#   WINE=/opt/wine-sg/bin/wine test/uiagroup-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_GROUP_ADD_FIRST_ONLY (uiautomationcore/uia_com_client.c).
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
T=$(mktemp -d /var/tmp/sg-uiagroup.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
LIBS="-luiautomationcore -lole32 -loleaut32 -luuid -loleacc -Wl,--allow-multiple-definition"
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/uiagroup64.exe" "$HERE/uiagroup-probe.c" $LIBS \
    || { echo "FAIL  probe did not build"; exit 1; }
ARCHS=64
if command -v "$MINGW32" >/dev/null; then
    TMPDIR=/var/tmp "$MINGW32" -O1 -o "$T/uiagroup32.exe" "$HERE/uiagroup-probe.c" $LIBS \
        || { echo "FAIL  32-bit probe did not build"; exit 1; }
    ARCHS="64 32"
fi
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/uiagroup*.exe "$WINEPREFIX/drive_c/"
rc=0
for a in $ARCHS; do
    cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" uiagroup$a.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe$a.out"
EOS
    chmod +x "$T/run.sh"
    timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
    echo "== $a-bit"
    cat "$T/probe$a.out"
    grep -qx 'RESULT: PASS' "$T/probe$a.out" || rc=1
done
exit $rc
