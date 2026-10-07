#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# COM classes served by a service, for a standard user (patches/sg/1302).
# Edge's App-Bound cookie encryption activates its elevation service's class
# (AppID LocalService = MicrosoftEdgeElevationService). Wine's COM started
# the service from the caller's process, and a standard user may not start
# services (the SCM's default descriptor): the start failed, Wine then ran a
# dllhost surrogate the class never asked for, dllhost could not load it and
# exited 0, and the caller waited 30 seconds for it -- on every attempt
# (David's Latitude 2026-10-06: pages took ~30 s to load; "class
# {1fcbe96c-...} not registered", syswow64\dllhost.exe). On Windows the DCOM
# launcher starts such a service for the caller, a class runs in a
# surrogate only if its AppID has a DllSurrogate value, and a surrogate
# that cannot load its class fails at once.
#  1. a standard user activates a LocalService class whose service it may
#     not start: rpcss (SYSTEM) starts it, the class object answers
#  2. a class with no server at all fails at once (REGDB_E_CLASSNOTREG, no
#     dllhost)
#  3. a surrogate class whose DLL is missing fails at once, not after 30 s
# Shared (system) prefix: this user owns it (= SYSTEM); SG_OTHER (default
# sgconf) is a standard user. Exit 77 without it or passwordless sudo.
# Mutants: SG_MUTANT_RPCSS_LOCAL_SERVICE, SG_MUTANT_SURROGATE_WITHOUT_VALUE
# (dlls/combase/rpc.c), SG_MUTANT_DLLHOST_EXIT_ZERO (programs/dllhost).
#   WINE=... WINESERVER=... test/comsvc-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/comsvc.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O2 -municode -o "$W/comsvc.exe" "$HERE/comsvc-probe.c" -lole32 -luuid -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W"/*.exe
mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
# the machine's services run as the owner (SYSTEM), kept up by an owner's
# process for the whole run -- one services.exe (see scm-access-gate.sh)
sg "$SG_GROUP" -c "umask 002; '$WINE' '$W/comsvc.exe' sleep" >/dev/null 2>&1 &
for i in $(seq 120); do [ -e "$PFX/system.reg" ] && [ -e "$PFX/drive_c/windows/system32/sc.exe" ] && break; sleep 1; done
sleep 5
chmod -R g+rwX "$PFX" 2>/dev/null
cp "$W/comsvc.exe" "$PFX/drive_c/comsvc.exe"
chmod 755 "$PFX/drive_c/comsvc.exe"

out=$("$WINE" 'C:\comsvc.exe' setup 2>/dev/null | tr -d '\r')
[ "$out" = "RESULT setup 0" ] && pass "the test service and classes are set up (as SYSTEM)" || fail "setup: $out"
other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME=/var/tmp \
        timeout -s KILL 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'
}
v() { printf '%s\n' "$1" | sed -n "s/^$2 //p"; }
sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME=/var/tmp \
    timeout -s KILL 120 "$WINE" sc start SgComSvc >/dev/null 2>&1; rc=$?
[ "$rc" = 5 ] && pass "the standard user may not start the service itself (sc start: 5)" || fail "sc start as the standard user: $rc"
"$WINE" sc stop SgComSvc >/dev/null 2>&1; sleep 1

out=$(other 'C:\comsvc.exe' get '{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550101}')
[ "$(v "$out" "RESULT get")" = 00000000 ] && pass "...but activating its class starts it (rpcss) and the class object answers ($(v "$out" MS) ms)" \
    || fail "activation as a standard user: $(v "$out" "RESULT get") after $(v "$out" MS) ms"

out=$(other 'C:\comsvc.exe' get '{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550102}')
ms=$(v "$out" MS)
[ "$(v "$out" "RESULT get")" = 80040154 ] && [ "${ms:-99999}" -lt 5000 ] \
    && pass "a class with no server fails at once: REGDB_E_CLASSNOTREG, no surrogate ($ms ms)" \
    || fail "no-server class: $(v "$out" "RESULT get") after $ms ms"

out=$(other 'C:\comsvc.exe' get '{6A1B2C3D-0001-4E5F-9A9A-5AB0C0550103}')
ms=$(v "$out" MS)
case "$(v "$out" "RESULT get")" in 0000000*|"") fail "surrogate class with no DLL: $(v "$out" "RESULT get")" ;;
    *) [ "${ms:-99999}" -lt 10000 ] && pass "a surrogate whose DLL is missing fails at once ($(v "$out" "RESULT get"), $ms ms, not 30 s)" \
        || fail "surrogate class waited $ms ms" ;; esac

echo
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL ($fails)"
[ "$fails" = 0 ]
