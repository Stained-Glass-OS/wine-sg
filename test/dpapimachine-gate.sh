#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# CryptProtectData's CRYPTPROTECT_LOCAL_MACHINE (wine-sg 7844). The computer's data may be unprotected by any
# of its users, as on Windows: Visual Studio 2022's setup (as SYSTEM) protects its license data so, under
# HKLM\Software\Classes\Licenses, and every user's Visual Studio unprotects it. Wine keyed every blob to the
# user who made it, so a user's IDE could not read it: "The license for Visual Studio has expired".
# A user's own data stays the user's. In a shared prefix, with SYSTEM ($SG_SYSTEM) and two users:
#   SYSTEM's machine data: both users unprotect it; SYSTEM's own data: a user cannot
#   a user's machine data: the other user unprotects it; a user's own data: the other cannot, the user can
#   WINE=... test/dpapimachine-gate.sh
# Mutant: crypt32 SG_MUTANT_DPAPI_MACHINE_IS_USER.
# Needs passwordless sudo, the accounts of usersessions-gate.sh and mingw; exit 77 otherwise.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_USER2=${SG_USER2:-sgtest2}; SG_GROUP=${SG_GROUP:-sgwine}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do
    id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || { echo "SKIP: no $u or no sudo"; exit 77; }
done
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/sg-dpapimachine.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
as() {
    local u=$1 ws=; shift
    [ "$u" = "$SG_SYSTEM" ] && ws='__wineservice_winstation\Default'
    sudo -n -u "$u" env ${ws:+SG_WINSTATION="$ws"} WINEPREFIX="$PFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
        WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$W/home-$u" XDG_RUNTIME_DIR="$W/run-$u" \
        WAYLAND_DISPLAY=sg-no-wayland DISPLAY= SG_WINE_REDIRECT=0 "$@"
}
cleanup() {
    set +e
    as "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "${KEEP:-}" ] || sudo -n rm -rf "$W"
}
trap cleanup EXIT
x86_64-w64-mingw32-gcc -O1 -o "$W/probe.exe" "$HERE/dpapimachine-probe.c" -lcrypt32 || { echo "FAIL build"; exit 1; }
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$W/home-$u" "$W/run-$u"; done
sudo -n install -d -m 1777 "$W/blobs"
sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$PFX"
sudo -n -u "$SG_SYSTEM" touch "$PFX/.sg-system-prefix"
as "$SG_SYSTEM" sh -c "umask 002; '$WINESERVER' -p"
as "$SG_SYSTEM" sh -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1

run() {   # run USER ARGS...: the probe's output line
    as "$1" sh -c 'cd "$1" && shift && timeout -s KILL 60 "$@"' _ "$W" "$WINE" "$W/probe.exe" "${@:2}" 2>/dev/null </dev/null | tr -d '\r' | head -1
}
blob() { printf 'Z:%s' "$(printf '%s' "$W/blobs/$1" | tr '/' '\\')"; }
expect() {   # expect WHAT WANT USER ARGS...
    local what=$1 want=$2 got; shift 2
    got=$(run "$@")
    case "$got" in "$want"*) pass "$what ($got)" ;; *) fail "$what: got '$got', wanted '$want'" ;; esac
}
[ "$(run "$SG_SYSTEM" protect "$(blob sysmachine)" machine)" = protect=OK ] || fail "SYSTEM could not protect machine data"
[ "$(run "$SG_SYSTEM" protect "$(blob sysuser)")" = protect=OK ] || fail "SYSTEM could not protect its own data"
[ "$(run "$SG_USER1" protect "$(blob u1machine)" machine)" = protect=OK ] || fail "$SG_USER1 could not protect machine data"
[ "$(run "$SG_USER1" protect "$(blob u1user)")" = protect=OK ] || fail "$SG_USER1 could not protect its own data"

expect "SYSTEM's machine data: $SG_USER1 unprotects it (Visual Studio's license)" "unprotect=OK sg-dpapi-secret" "$SG_USER1" unprotect "$(blob sysmachine)"
expect "SYSTEM's machine data: $SG_USER2 unprotects it" "unprotect=OK sg-dpapi-secret" "$SG_USER2" unprotect "$(blob sysmachine)"
expect "SYSTEM's machine data: SYSTEM itself too" "unprotect=OK sg-dpapi-secret" "$SG_SYSTEM" unprotect "$(blob sysmachine)"
expect "SYSTEM's own data: $SG_USER1 cannot" "unprotect=FAIL" "$SG_USER1" unprotect "$(blob sysuser)"
expect "SYSTEM's own data: SYSTEM can" "unprotect=OK sg-dpapi-secret" "$SG_SYSTEM" unprotect "$(blob sysuser)"
expect "$SG_USER1's machine data: $SG_USER2 unprotects it" "unprotect=OK sg-dpapi-secret" "$SG_USER2" unprotect "$(blob u1machine)"
expect "$SG_USER1's own data: $SG_USER2 cannot" "unprotect=FAIL" "$SG_USER2" unprotect "$(blob u1user)"
expect "$SG_USER1's own data: $SG_USER1 can" "unprotect=OK sg-dpapi-secret" "$SG_USER1" unprotect "$(blob u1user)"

[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
