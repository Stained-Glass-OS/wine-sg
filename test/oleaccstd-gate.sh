#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# oleacc's standard objects (patches/sg/1687), on Xvfb (Decorated=N):
# test/oleaccstd-probe.c reads a window through MSAA: its OBJID_WINDOW object
# (role, name, state, parts, navigation, hit test), its title bar, menu bar,
# scroll bar and size grip objects (roles, buttons, value, default actions),
# the cursor and caret objects, and a button's role and default action.
#
#   WINE=/opt/wine-sg/bin/wine test/guires-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_PARTS, SG_MUTANT_NO_PART_ACTION (oleacc/nonclient.c),
# SG_MUTANT_NO_BUTTON_ACTION (oleacc/client.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-oleaccstd.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/oleaccstd-probe.exe" "$HERE/oleaccstd-probe.c" -luser32 -lgdi32 -lole32 -loleaut32 -luuid -loleacc -Wl,--allow-multiple-definition \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\X11 Driver' /v Decorated /d N /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/oleaccstd-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" oleaccstd-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
