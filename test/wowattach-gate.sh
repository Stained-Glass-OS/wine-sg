#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A 64-bit debugger attaching to a running 32-bit program does not kill it (patches/sg/7002).
# DbgUiIssueRemoteBreakin asks the program for a thread at the 64-bit ntdll's DbgUiRemoteBreakin;
# a wow64 process starts every thread in 32-bit code, so the address was cut to 32 bits and the
# thread faulted (winedbg attached to Pidgin: "page fault on execute access to 0xffbcffac", and
# the program died). It now starts at the 32-bit ntdll's DbgUiRemoteBreakin. The probe
# (test/wowattach-probe.c, 64-bit) attaches to a 32-bit child and to a 64-bit one (control),
# expects the break-in breakpoint, detaches, and the child then exits with its own code.
#   WINE=/opt/wine-sg/bin/wine test/wowattach-gate.sh
# Mutant: SG_MUTANT_BREAKIN_64BIT_IN_WOW64 (dlls/ntdll/unix/server.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wowattach.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe64.exe" "$HERE/wowattach-probe.c" || { fail "probe (64-bit) did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/probe32.exe" "$HERE/wowattach-probe.c" || { fail "probe (32-bit) did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe64.exe' 'C:\probe32.exe' 'C:\probe64.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
}
check wow64_child_wow64 "the 32-bit child runs under wow64"
check wow64_attached    "a 64-bit debugger attaches to the 32-bit program"
check wow64_breakin     "the break-in thread reaches its breakpoint (no fault)"
check wow64_detached    "the debugger detaches"
check wow64_survived    "the 32-bit program keeps running and exits with its own code"
check native_attached   "control: attaching to a 64-bit program"
check native_breakin    "control: its break-in breakpoint"
check native_survived   "control: the 64-bit program survives"
check done              "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
