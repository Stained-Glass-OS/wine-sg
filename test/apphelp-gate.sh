#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# apphelp.dll (patches/sg/5502): ShimFlushCache needs an administrator (the
# prefix owner succeeds, the same call under a token with Administrators
# disabled fails with ERROR_ACCESS_DENIED -- after the gate has checked that
# token really is not an administrator), SdbGetAppPatchDir gives
# <windows>\AppPatch or the empty string. 64- and 32-bit.
#
#   WINE=/opt/wine-sg/bin/wine test/apphelp-gate.sh
# Mutants (apphelp.c; build with x86_64_CFLAGS / i386_CFLAGS = "-g -O2 -DSG_MUTANT_X"):
# SG_MUTANT_NO_ADMIN_CHECK, SG_MUTANT_PATCHDIR_EMPTY, SG_MUTANT_PATCHDIR_NO_SUFFIX,
# SG_MUTANT_PATCHDIR_NOT_EMPTY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-apphelp.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
x86_64-w64-mingw32-gcc -O2 -o "$T/ap64.exe" "$HERE/apphelp-probe.c" -ladvapi32 || { echo "FAIL  probe"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/ap32.exe" "$HERE/apphelp-probe.c" -ladvapi32 || { echo "FAIL  probe"; exit 1; }
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

for b in 64 32; do
    out=$(DISPLAY= timeout -s KILL 120 "$WINE" "$T/ap$b.exe" 2>/dev/null | tr -d '\r')
    has() { printf '%s\n' "$out" | grep -qxF -- "$1"; }
    has "is-admin=1" && pass "$b-bit the prefix owner is an administrator" || fail "$b-bit is-admin: $out"
    has "flush-admin=1 error=0" && pass "$b-bit ShimFlushCache succeeds for an administrator" || fail "$b-bit flush-admin: $(printf '%s' "$out" | grep flush-admin)"
    has "restricted-is-admin=0" && pass "$b-bit the restricted token is not an administrator (the check below means something)" ||
        fail "$b-bit restricted token: $(printf '%s' "$out" | grep restricted)"
    has "flush-user=0 error=5" && pass "$b-bit ShimFlushCache fails with ERROR_ACCESS_DENIED for a user" || fail "$b-bit flush-user: $(printf '%s' "$out" | grep flush-user)"
    has "patchdir-ok=1" && pass "$b-bit SdbGetAppPatchDir is <windows>\\AppPatch" || fail "$b-bit patchdir: $(printf '%s' "$out" | grep patchdir-ok)"
    has "patchdir-small-empty=1" && pass "$b-bit a buffer that is too small gives the empty string" || fail "$b-bit small buffer: $(printf '%s' "$out" | grep small)"
    has "patchdir-zero-untouched=1" && pass "$b-bit a zero size writes nothing" || fail "$b-bit zero size: $(printf '%s' "$out" | grep zero)"
done

[ $RC = 0 ] && echo "apphelp-gate: all passed" || echo "apphelp-gate: FAILURES"
exit $RC
