#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window's property store and AppUserModelIDs group taskbar buttons
# (patches/sg/1640): test/winappid-probe.c, in the shell's desktop beside
# explorer's taskbar (under Xvfb, buttons combined), gives its windows IDs
# through SHGetPropertyStoreForWindow, reads them back (also from another
# process), and counts the taskbar's buttons for them: different IDs split
# one program's windows, the same ID -- also another process's explicit one
# -- joins them. The store kept nothing before.
#
#   WINE=/opt/wine-sg/bin/wine test/winappid-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_GROUP_BY_EXE (programs/explorer/systray.c),
# SG_MUTANT_NO_PROCESS_AUMID_SHARED (shcore/main.c),
# SG_MUTANT_PROPSTORE_ANY_WINDOW (shell32/shell32_main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-winappid.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/winappid-probe.exe" "$HERE/winappid-probe.c" -lole32 -luuid -lshell32 -lpropsys -luser32 -lgdi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/winappid-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced' /v TaskbarGlomLevel /t REG_DWORD /d 0 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
timeout -s KILL 180 "$WINE" winappid-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
