#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Security.EnterpriseData.ProtectionPolicyManager (patches/sg/1526),
# as Windows answers it where Windows Information Protection manages no
# identity: the class is registered and activatable; no identity is managed,
# protection is off (enforcement NoProtection), access is allowed, user
# decryption is allowed, there is no primary managed identity; the manager
# for the current view keeps the identity it is given, and a thread network
# context closes. Office looked the class up as it started and opened files
# ("GetIPolicyProtectionMgrStatics2 0x80040154"), and Excel and OneNote
# then failed fast.
#
#   WINE=/opt/wine-sg/bin/wine test/edppolicy-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null && command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-edppolicy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/p64.exe" "$HERE/edppolicy-probe.c" || { fail "probe did not build"; exit 1; }
i686-w64-mingw32-gcc -O2 -o "$T/p32.exe" "$HERE/edppolicy-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
WANT="factory=0 managed=0 enabled=0 level=0 check=0 decrypt=1 view=0 identity=user@contoso.com context=0 primary=null"
for a in 64 32; do
    O=$(timeout 60 "$WINE" "$T/p$a.exe" 2>/dev/null | tr -d '\r')
    echo "      $a: $O"
    case "$O" in "factory=0 "*) pass "$a-bit: the class is registered and its statics activate" ;; *) fail "$a-bit factory: $O" ;; esac
    [ "$O" = "$WANT" ] && pass "$a-bit: no identity managed, protection off, access allowed, the view's manager keeps its identity" ||
        fail "$a-bit: $O"
done
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
