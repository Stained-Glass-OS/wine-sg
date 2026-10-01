#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A COM proxy's security blanket (patches/sg/0627). IClientSecurity on a
# proxy was a stub that answered E_NOTIMPL, so CoSetProxyBlanket failed, and
# Omaha's updaters stop on that: Brave's standalone installer put its updater
# in place and then failed with 0x80004001. Now a proxy keeps its blanket:
# setting it succeeds and calls still go through, a query returns what was
# set, the proxy's own IMarshal has none (E_NOINTERFACE), an unknown
# authentication service is E_INVALIDARG, and CoCopyProxy gives a proxy.
# An object in a single-threaded apartment called from the multithreaded
# one; 64-bit and 32-bit (WoW64), as Omaha is.
#
#   WINE=/opt/wine-sg/bin/wine test/proxyblanket-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for a in x86_64 i686; do
    command -v "$a-w64-mingw32-gcc" >/dev/null || { echo "SKIP: $a mingw-w64 not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-proxyblanket.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp "$a-w64-mingw32-gcc" -O2 -o "$T/pb-$a.exe" "$HERE/proxyblanket-probe.c" -lole32 -luuid ||
        { fail "probe did not build ($a)"; exit 1; }
done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T"/pb-*.exe "$WINEPREFIX/drive_c/"
# v OUTPUT KEY: the value of KEY= on any line
v() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }
for a in x86_64 i686; do
    out=$(timeout 60 "$WINE" "C:\\pb-$a.exe" 2>/dev/null | tr -d '\r')
    [ "$(v "$out" setdefault)" = 00000000 ] && pass "$a: CoSetProxyBlanket as Omaha calls it succeeds" ||
        { fail "$a: CoSetProxyBlanket $(v "$out" setdefault)${out:+ ($(printf '%s' "$out" | head -1))}"; continue; }
    [ "$(v "$out" query)" = 00000000 ] && [ "$(v "$out" level)" = 6 ] && [ "$(v "$out" imp)" = 3 ] &&
        [ "$(v "$out" authn)" = 10 ] && [ "$(v "$out" querynull)" = 00000000 ] &&
        pass "$a: a query returns the blanket set (packet privacy, impersonate; NTLM kept)" ||
        fail "$a: query $(v "$out" query) authn $(v "$out" authn) level $(v "$out" level) imp $(v "$out" imp) null $(v "$out" querynull)"
    [ "$(v "$out" call)" = 80004002 ] && [ "$(v "$out" calls)" = 1 ] && pass "$a: a call through the proxy reaches the object" ||
        fail "$a: call $(v "$out" call) calls $(v "$out" calls)"
    [ "$(v "$out" local)" = 80004002 ] && [ "$(v "$out" invalid)" = 80070057 ] &&
        pass "$a: the proxy's own IMarshal is E_NOINTERFACE; an unknown service E_INVALIDARG" ||
        fail "$a: local $(v "$out" local) invalid $(v "$out" invalid)"
    [ "$(v "$out" copy)" = 00000000 ] && [ "$(v "$out" copied)" = 1 ] && pass "$a: CoCopyProxy gives a proxy" ||
        fail "$a: copy $(v "$out" copy) copied $(v "$out" copied)"
done
exit $RC
