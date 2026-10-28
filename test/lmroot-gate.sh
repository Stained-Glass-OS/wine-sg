#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# The machine's roots are in a standard user's 32-bit programs' Root store (patches/sg/7604).
#
# A user's Root store holds the machine's, opened for writing first: for a 32-bit program
# of a standard user, registry virtualization (3764) gave the empty VirtualStore copy instead
# of failing, and the store had 5 roots of the machine's 249 -- GOM Player's signatures did not
# chain (CERT_E_CHAINING) though SYSTEM had added their root. As SYSTEM, a test root goes into
# the machine's Root store; a standard user's 64- and 32-bit programs (no manifest: the ones
# virtualized) must both find it, as on Windows (the user's view of the machine store is
# read-only). Accounts as test/usersessions-gate.sh; exit 77 without them.
# Mutant: SG_MUTANT_MACHINE_STORE_WRITABLE (dlls/crypt32/store.c).
#   WINE=... WINESERVER=... test/lmroot-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_GROUP=${SG_GROUP:-sgwine}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1"; do id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }; done
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc openssl; do command -v $t >/dev/null || { echo "SKIP: no $t"; exit 77; }; done
W=$(mktemp -d /var/tmp/lmroot.XXXXXX); chmod 755 "$W"
PFX=$W/prefix
as() { local u=$1 ws=; shift; [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" "$@"; }
cleanup() { set +e; as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1; [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"; }
trap cleanup EXIT
x86_64-w64-mingw32-gcc -O1 -o "$W/p64.exe" "$HERE/lmroot-probe.c" -lcrypt32 && i686-w64-mingw32-gcc -O1 -o "$W/p32.exe" "$HERE/lmroot-probe.c" -lcrypt32 \
    || { echo "FAIL build"; exit 1; }
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$W/ca.key" -out "$W/ca.pem" -days 3650 -subj "/CN=SG Gate Machine Root" \
    -addext "basicConstraints=critical,CA:TRUE" >/dev/null 2>&1 && openssl x509 -in "$W/ca.pem" -outform DER -out "$W/ca.der" || { echo "FAIL cert"; exit 1; }
for u in "$SG_SYSTEM" "$SG_USER1"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u"; done
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
for f in p64.exe p32.exe ca.der; do sudo -n install -m 755 -o "$SG_SYSTEM" -g "$SG_GROUP" "$W/$f" "$PFX/drive_c/$f"; done
out=$(as "$SG_SYSTEM" sh -c "umask 002; timeout 120 '$WINE' 'C:\\p64.exe' add 'C:\\ca.der'" 2>/dev/null | tr -d '\r')
[ "$out" = ADDED ] || { fail "SYSTEM could not add the test root: $out"; echo "RESULT: FAIL"; exit 1; }
sudo -n chmod -R g+rwX "$PFX" 2>/dev/null
as "$SG_USER1" timeout -s KILL 120 "$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f >/dev/null 2>&1
r64=$(as "$SG_USER1" timeout 120 "$WINE" 'C:\p64.exe' find 'SG Gate Machine Root' 2>/dev/null | tr -d '\r')
r32=$(as "$SG_USER1" timeout 120 "$WINE" 'C:\p32.exe' find 'SG Gate Machine Root' 2>/dev/null | tr -d '\r')
echo "      64-bit: $r64; 32-bit: $r32"
case "$r64" in "FOUND 1 "*) pass "a standard user's 64-bit program has the machine's root" ;; *) fail "64-bit: $r64" ;; esac
case "$r32" in "FOUND 1 "*) pass "...and a 32-bit one too (registry virtualization does not hide the machine's store)" ;; *) fail "32-bit: $r32" ;; esac
echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
