#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# win32u: a process that may start no others still has its desktop window, with no shell, and no error
# (patches/sg/9803). Chromium's and QtWebEngine's sandboxed children (Zeal, Joplin, Zulip, Element) run
# on an alternate desktop with PROCESS_CREATION_CHILD_PROCESS_RESTRICTED; Wine starts an explorer for a
# desktop, the server refuses it (c000049d) and each child logged "err:win:get_desktop_window failed to
# start explorer". Windows makes desktop windows in win32k with no process, which is what the desktop
# window made after the refused start already is. 64- and 32-bit. test/noshelldesk-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/noshelldesk-gate.sh
# Mutant (dlls/win32u/winstation.c): SG_MUTANT_SANDBOX_DESKTOP_ERR -> the error is back.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-noshelldesk.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/noshelldesk-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/noshelldesk-probe.c" ||
    { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T"/p*.exe "$WINEPREFIX/drive_c/"
RC=0
for p in p64 p32; do
    out=$(cd "$WINEPREFIX/drive_c" && WINEDEBUG=-all,err+win timeout -s KILL 120 "$WINE" $p.exe 2>"$T/$p.err" </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      /"
    sed 's/^/      stderr: /' "$T/$p.err" | head -5
    chk() { if printf '%s\n' "$out" | grep -q "$1"; then printf 'PASS  %s: %s\n' $p "$2"; else printf 'FAIL  %s: %s\n' $p "$2"; RC=1; fi; }
    chk '^create_desktop=1$' "the parent makes the alternate desktop"
    chk '^child_desktop=sg-noshell$' "the child runs on it"
    chk '^child_create=0 367$' "the child may start no process (ERROR_CHILD_PROCESS_BLOCKED)"
    chk '^desktop_window=1 1$' "the child has a desktop window"
    chk '^desktop_rect=1$' "  with the screen's size"
    chk '^child_exit=0$' "the child finished"
    if grep -q 'failed to start explorer\|failed to create desktop window' "$T/$p.err"; then
        printf 'FAIL  %s: no error for the shell-less desktop\n' $p; RC=1
    else printf 'PASS  %s: no error for the shell-less desktop\n' $p; fi
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
