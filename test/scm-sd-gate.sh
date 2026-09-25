#!/bin/bash
# Gate for wine-sg 0185: a service's own security descriptor.
# `sc sdshow` / `sc sdset`, QueryServiceObjectSecurity/SetServiceObjectSecurity:
# the descriptor is kept as Windows keeps it (Services\<name>\Security,
# value Security) and the SCM's access checks (0141) use it instead of the
# default -- so an administrator can let standard users start a service, or
# stop them from even querying it.
#
# Shared (system) prefix: this user owns it (= SYSTEM); SG_OTHER (default
# sgconf) is a standard user. Exit 77 without it or passwordless sudo.
#   WINE=... WINESERVER=... test/scm-sd-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/scmsd.XXXXXX)
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
# one services.exe for the whole run (see scm-access-gate.sh)
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
osc() { "$WINE" sc "$@" 2>/dev/null | tr -d '\r'; }
xsc() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
        "$WINE" sc "$@" 2>/dev/null | tr -d '\r'
}
expect() {  # who op want args...
    local who=$1 op=$2 want=$3 got
    shift 3
    got=$($who "$op" "$@" | sed -n "s/^RESULT $op //p")
    if [ "$got" = "$want" ]; then pass "$who $op $* -> $want"; else fail "$who $op $* -> '$got', want $want"; fi
}

expect owner create 0 sgsdsvc 'C:\scm-svc.exe'
out=$(osc sdshow sgsdsvc)
case "$out" in *"D:"*";;;AU)"*";;;SY)"*";;;BA)"*) pass "sc sdshow: Windows' default (AU, SY, BA): $(echo $out)" ;;
    *) fail "sc sdshow: '$out'" ;; esac
out=$(xsc sdshow sgsdsvc)
case "$out" in *"D:"*";;;AU)"*) pass "a standard user may read it (READ_CONTROL)" ;; *) fail "standard user's sdshow: '$out'" ;; esac
expect other start 5 sgsdsvc
out=$(xsc sdset sgsdsvc 'D:(A;;CCLCSWRPWPDTLOCRRC;;;AU)')
case "$out" in *"FAILED 5"*) pass "a standard user may not change it (OpenService FAILED 5)" ;; *) fail "standard user's sdset: '$out'" ;; esac

# the administrator lets everyone start and stop it (Windows' letters: RP start, WP stop)
SDDL='D:(A;;CCLCSWRPWPDTLOCRRC;;;AU)(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;SY)(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)'
out=$(osc sdset sgsdsvc "$SDDL")
case "$out" in *"SetServiceObjectSecurity SUCCESS"*) pass "sc sdset as SYSTEM: SUCCESS" ;; *) fail "sdset: '$out'" ;; esac
out=$(osc sdshow sgsdsvc)
case "$out" in *"(A;;CCLCSWRPWPDTLOCRRC;;;AU)"*) pass "sc sdshow shows the new DACL" ;; *) fail "sdshow after sdset: '$out'" ;; esac
sec=$("$WINE" reg query 'HKLM\System\CurrentControlSet\Services\sgsdsvc\Security' /v Security 2>/dev/null | tr -d '\r' | grep -c REG_BINARY)
[ "$sec" = 1 ] && pass "kept as Windows keeps it: Services\\sgsdsvc\\Security, Security (REG_BINARY)" || fail "no Security value"
expect other start 0 sgsdsvc
sleep 2
[ "$(owner state sgsdsvc | sed -n 's/^STATE //p')" = 4 ] && pass "the standard user started it (RUNNING; the service still reports its state)" \
    || fail "not running after the standard user's start"
expect other stop 0 sgsdsvc
sleep 2
expect other delete 5 sgsdsvc
expect other config 5 sgsdsvc disabled

# it survives the SCM: a new services.exe reads it from the registry
"$WINESERVER" -k; sleep 2
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' '$W/scm-probe.exe' sleep" >/dev/null 2>&1 &
sleep 6
expect other start 0 sgsdsvc
sleep 2
expect owner stop 0 sgsdsvc
sleep 2

# and it can shut a standard user out entirely
out=$(osc sdset sgsdsvc 'D:(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;SY)(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)')
case "$out" in *SUCCESS*) pass "sdset: SYSTEM and Administrators only" ;; *) fail "sdset 2: '$out'" ;; esac
expect other query 5 sgsdsvc
expect other open 5 sgsdsvc 0x20000
expect owner query 0 sgsdsvc
expect owner start 0 sgsdsvc
sleep 2
expect owner stop 0 sgsdsvc
sleep 2
expect owner delete 0 sgsdsvc

echo "scm-sd-gate: $fails failure(s)"
[ $fails -eq 0 ]
