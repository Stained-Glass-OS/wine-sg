#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WScript.Shell (patches/sg/1647): test/wshshell.vbs under cscript, in the
# shell's desktop (Xvfb): the process, user, volatile and system
# environments (set, read, list, remove), SpecialFolders by position and
# name, shortcuts (an existing one read by CreateShortcut, hotkeys, target,
# full name, Load), RegDelete, LogEvent, Exec's exit code, and AppActivate
# and SendKeys into a window (test/wshshell-target.c). These were E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/wshshell-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_PROCESS_ENV_ONLY, SG_MUTANT_NO_SENDKEYS
# (wshom.ocx/shell.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-wshshell.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wshshell-target.exe" "$HERE/wshshell-target.c" || { echo "FAIL  target did not build"; exit 1; }
cp "$HERE/wshshell.vbs" "$T/"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

ZT='Z:'"$(printf '%s' "$T" | tr / '\\')"
cat > "$T/session.sh" <<EOF
#!/bin/sh
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
timeout -s KILL 240 "$WINE" cscript //nologo '$ZT\\wshshell.vbs' '$ZT' 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
