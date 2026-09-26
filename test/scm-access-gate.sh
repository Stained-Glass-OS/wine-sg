#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0141: the Service Control Manager checks who is asking.
# Wine's SCM granted every handle whatever access was asked for, so in a
# shared prefix any standard user could stop, reconfigure, delete or create
# services. Now: everyone may connect, enumerate and query; only SYSTEM and
# Administrators may start/stop/pause, change, delete or create -- except
# that Wine's own services (a program in system32) may be started by anyone,
# because Wine starts RpcSs, MSIServer, COM servers... from the caller.
#
# Shared (system) prefix: this user owns it (= SYSTEM); SG_OTHER (default
# sgconf) is a standard user. Exit 77 without it or passwordless sudo.
#   WINE=... WINESERVER=... test/scm-access-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/scmacc.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }

cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O2 -municode -o "$W/scm-svc.exe" "$HERE/scm-svc.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
"$MINGW" -O2 -o "$W/scm-probe.exe" "$HERE/scm-probe.c" -ladvapi32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W"/*.exe

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
# The machine's services run as the owner (SYSTEM), as sg-services-start keeps
# them on the image: a process of the owner's, started first (it initialises
# the prefix), keeps them up for the whole run. Started after a wineboot that
# exited, a second services.exe came up beside the exiting one, each with its
# own database -- every other call then failed with 1060 (stock too).
sg "$SG_GROUP" -c "umask 002; '$WINE' '$W/scm-probe.exe' sleep" >/dev/null 2>&1 &
for i in $(seq 120); do [ -e "$PFX/system.reg" ] && [ -e "$PFX/drive_c/windows/system32/sc.exe" ] && break; sleep 1; done
sleep 5
chmod -R g+rwX "$PFX" 2>/dev/null
cp "$W/scm-svc.exe" "$PFX/drive_c/scm-svc.exe"

owner() { "$WINE" "$W/scm-probe.exe" "$@" 2>/dev/null | tr -d '\r'; }
other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
        "$WINE" "$W/scm-probe.exe" "$@" 2>/dev/null | tr -d '\r'
}
expect() {  # who op want args...
    local who=$1 op=$2 want=$3 got
    shift 3
    got=$($who "$op" "$@" | sed -n "s/^RESULT $op //p")
    if [ "$got" = "$want" ]; then pass "$who $op $* -> $want"; else fail "$who $op $* -> '$got', want $want"; fi
}

expect owner create 0 sgtestsvc 'C:\scm-svc.exe'
expect other create 5 sgbadsvc 'C:\scm-svc.exe'
expect other enum 0
expect other enum 0
expect owner enum 0
expect other query 0 sgtestsvc
expect other start 5 sgtestsvc
expect owner start 0 sgtestsvc
sleep 2
[ "$(owner state sgtestsvc | sed -n 's/^STATE //p')" = 4 ] && pass "the owner started it (RUNNING)" || fail "not running after the owner's start"
expect other stop 5 sgtestsvc
expect other pause 5 sgtestsvc
expect other config 5 sgtestsvc disabled
expect other delete 5 sgtestsvc
expect other open 0 sgtestsvc 0x2018d
expect other open 0 sgtestsvc 0x2000000
expect other open 5 sgtestsvc 0xf01ff
expect owner pause 0 sgtestsvc
expect owner continue 0 sgtestsvc
expect owner stop 0 sgtestsvc
sleep 2
[ "$(owner state sgtestsvc | sed -n 's/^STATE //p')" = 1 ] && pass "the owner stopped it (STOPPED)" || fail "not stopped after the owner's stop"
expect owner config 0 sgtestsvc auto
expect other open 0 RpcSs 0x10
expect other open 5 RpcSs 0x20
expect owner delete 0 sgtestsvc

echo "scm-access-gate: $fails failure(s)"
[ $fails -eq 0 ]
