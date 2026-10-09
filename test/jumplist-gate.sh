#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Jump lists (patches/sg/1664), in the shell's desktop (Xvfb):
# test/jumplist-probe.c builds a list with ICustomDestinationList (a
# category, the Recent category, tasks with a separator) in an
# EnumerableObjectCollection, puts documents on its list with
# SHAddToRecentDocs(SHARD_APPIDINFO), reads them back through
# IApplicationDocumentLists, removes them with IApplicationDestinations,
# and runs a task from its taskbar button's menu. These were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/jumplist-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_COMMIT_NOTHING (shell32/jumplist.c),
# SG_MUTANT_NO_APP_RECENT (shell32/shellord.c), SG_MUTANT_NO_JUMP_MENU
# (explorer/systray.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-jumplist.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/jumplist-probe.exe" "$HERE/jumplist-probe.c" -municode -luser32 -lole32 -ladvapi32 -lshell32 -lpropsys -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/jumplist-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
timeout -s KILL 180 "$WINE" jumplist-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
