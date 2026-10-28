#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's own pseudoconsole host started as ours (patches/sg/6181), as Windows Terminal's ConPTY code
# does it: ConDrv's server opened without synchronous I/O and "\Reference" below it (a leading backslash
# relative to the device's file); its OpenConsole.exe "--headless --textMeasurement ... --signal --server"
# started as our conhost.exe; a synchronous signal pipe (a resize sent); a client on the pseudoconsole
# named by GetModuleFileNameEx at once; its output read by the program. test/ptyhost-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/ptyhost-gate.sh
# Mutants: SG_MUTANT_DEVICE_RELATIVE_BACKSLASH (dlls/ntdll/unix/file.c), SG_MUTANT_NO_CONSOLE_HOST_REDIRECT
# (dlls/kernelbase/process.c), SG_MUTANT_NO_IMAGE_NAME_FALLBACK (dlls/kernelbase/debug.c),
# SG_MUTANT_CONHOST_STRICT_OPTIONS, SG_MUTANT_CONHOST_SYNC_SIGNAL (programs/conhost/conhost.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-ptyhost.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/ptyhost-probe.exe" "$HERE/ptyhost-probe.c" -lpsapi || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/ptyhost-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" ptyhost-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk '^server=00000000$' "ConDrv's server opened without synchronous I/O"
chk '^reference=00000000$' "\\Reference below the server (a name with a leading backslash relative to the device's file)"
chk '^host=1,0$' "the program's OpenConsole.exe runs as our conhost.exe"
chk '^client=1,1$' "a client on the pseudoconsole, named at once by GetModuleFileNameEx"
chk '^echo=1$' "its output reaches the program through our host (a synchronous signal pipe with a resize)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
