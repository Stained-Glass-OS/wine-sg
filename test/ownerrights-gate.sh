#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# OWNER RIGHTS ACEs are the owner's (patches/sg/0622). CPython 3.12.4's
# os.mkdir(path, 0o700) -- tempfile.mkdtemp, BleachBit's settings folder --
# makes a directory whose DACL grants SYSTEM, the administrators and OWNER
# RIGHTS (S-1-3-4); Wine matched OWNER RIGHTS to no one, and a standard user
# got a mode-000 directory it could not write in. Here the DACL grants only
# SYSTEM and OWNER RIGHTS, so the maker gets in by OWNER RIGHTS alone.
#
#   WINE=/opt/wine-sg/bin/wine test/ownerrights-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-ownerrights.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -municode -o "$T/ownerrights-probe.exe" "$HERE/ownerrights-probe.c" ||
    { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/ownerrights-probe.exe" "$WINEPREFIX/drive_c/"
v() { printf '%s\n' "$1" | sed -n "s/^$2=//p"; }
out=$(timeout 60 "$WINE" 'C:\ownerrights-probe.exe' 'C:\owner-rights' 'D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;OW)' 2>/dev/null | tr -d '\r')
[ "$(v "$out" mkdir)" = 1 ] && pass "the directory is made" || fail "mkdir: $(v "$out" mkdir)"
[ "$(v "$out" create)" = "ok error=0" ] && pass "its maker writes a file in it (by OWNER RIGHTS)" || fail "create: $(v "$out" create)"
[ "$(v "$out" list)" = ok ] && pass "and lists it" || fail "list: $(v "$out" list)"
[ "$(v "$out" dac)" = ok ] && pass "and may read and change its security" || fail "security: $(v "$out" dac)"
mode=$(stat -c %A "$WINEPREFIX/drive_c/owner-rights" 2>/dev/null)
case "$mode" in drwx*) pass "on disk it is its owner's ($mode)" ;; *) fail "on disk: $mode" ;; esac
# what CPython makes (with the administrators): the same for its maker
out=$(timeout 60 "$WINE" 'C:\ownerrights-probe.exe' 'C:\py-mkdir' 2>/dev/null | tr -d '\r')
[ "$(v "$out" create)" = "ok error=0" ] && pass "CPython's 0o700 directory: its maker writes in it" || fail "CPython's: $(v "$out" create)"
exit $RC
