#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# bcdboot.exe in system32 (patches/sg/7907). Windows always has it; Rufus reads its version
# (GetExecutableVersion on %windir%\sysnative\bcdboot.exe, then version_to_uint64 without a NULL
# check) to know which options it takes, and with no file there it crashed at start (a read at
# address 2). The tool is there, in system32 and syswow64, with a version resource (10.0.19041);
# run, it answers as Windows' does when it cannot copy boot files (exit 1): the machine starts
# through Linux's boot loader. test/bcdboot-probe.c, 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/bcdboot-gate.sh
# Mutant: -DSG_MUTANT_BCDBOOT_NO_VERSION (programs/bcdboot/version.rc: no version resource).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bcdboot.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/bcdboot-probe.c" -lversion || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^$1=1$"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep -o "^$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check exists    "bcdboot.exe is in the system directory"
    check version   "it has a version resource (10.x)"
    check sysnative "it is read through %windir%\\sysnative from a 32-bit process too"
    check fails     "run, it says it cannot copy boot files (exit 1)"
    check done      "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
