#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A standard user trusts the machine's root certificates (patches/sg/0447).
#
# A user's Root store is the user's own plus the machine's; crypt32 opened the
# machine's for writing, which a standard user may not do, and left it out --
# every HTTPS server was then untrusted (wininet 12045, 12057): Get a web
# browser, installers' downloads, all failed for a standard user. Here the
# machine's Root key is made read-only, as a standard user sees it, and the
# user's Root store must still hold the machine's roots.
#
#   WINE=/opt/wine-sg/bin/wine test/userroots-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
ls /etc/ssl/certs/ISRG_Root_X1.pem >/dev/null 2>&1 || { echo "SKIP: the host has no ISRG Root X1 to import"; exit 77; }

T=$(mktemp -d /var/tmp/sg-userroots.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/userroots-probe.exe" "$HERE/userroots-probe.c" -lcrypt32 -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
P() { "$WINE" "$T/userroots-probe.exe" "$@" 2>/dev/null | tr -d '\r'; }

before=$(P find); echo "      as the prefix's owner: $before"
case "$before" in "count=21 isrg=1") pass "the machine's roots are imported and trusted" ;; *) fail "before: $before" ;; esac
lock=$(P lock); echo "      $lock"
case "$lock" in *"write=5") pass "the machine's Root key is read-only now, as for a standard user" ;; *) fail "could not lock the key: $lock" ;; esac
after=$(P find); echo "      read-only: $after"
case "$after" in "count=21 isrg=1") pass "the user still trusts the machine's roots" ;; *) fail "the user's Root store without write access: $after" ;; esac

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
