#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Window hot keys and ShellExecuteEx masks (patches/sg/1657), in the
# shell's desktop (Xvfb): test/hotkeyexec-probe.c starts itself with
# STARTF_USEHOTKEY and with ShellExecuteEx SEE_MASK_HOTKEY (the first window
# gets the hot key), presses the hot key (the window gets SC_HOTKEY and the
# foreground), checks WM_SETHOTKEY's answers, and opens a file's property
# sheet with ShellExecuteEx("properties", SEE_MASK_INVOKEIDLIST). These did
# nothing.
#
#   WINE=/opt/wine-sg/bin/wine test/hotkeyexec-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_SC_HOTKEY (win32u/message.c),
# SG_MUTANT_NO_MENU_VERBS (shell32/shlexec.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-hotkeyexec.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/hotkeyexec-probe.exe" "$HERE/hotkeyexec-probe.c" -mwindows -lshell32 || { echo "FAIL  probe did not build"; exit 1; }
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
cd "$T" && timeout -s KILL 240 "$WINE" '$ZT\\hotkeyexec-probe.exe' 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
