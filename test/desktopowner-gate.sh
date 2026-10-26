#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The desktop window is not explorer.exe's to other programs (patches/sg/6119): on Windows it is the
# system's (csrss.exe, not one programs can open). Told explorer, NVDA loaded its explorer app module
# while caching the desktop at startup, before its IAccessible support: a circular import, and no
# explorer support all session. wineboot still finds explorer through the desktop's thread.
# test/desktopowner-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/desktopowner-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (user32 win.c): SG_MUTANT_DESKTOP_OWNER_EXPLORER. The drop-target side (ole32 ole2.c
# SG_MUTANT_DROPTARGET_BY_WINDOW_PID: a file dragged from File Explorer onto the desktop) is test-drag's.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-desktopowner.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/desktopowner-probe.exe" "$HERE/desktopowner-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/desktopowner-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" desktopowner-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1 1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk TID "the desktop window has a thread"
chk NOTEXPLORER "its process, to another program, is not explorer.exe (nor the program)"
chk NOTOPEN "that process cannot be opened (as csrss.exe cannot)"
chk NOTLISTED "and is no listed process: no app module found for it"
chk SHORT "a 16-bit desktop handle gets the same answer"
chk THREADPROCESS "the desktop's thread is explorer's (wineboot finds the desktop process so)"
chk OWN "a program's own window is still its own"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
