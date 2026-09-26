#!/bin/bash
# A Windows program sets the time zone and the clock through sg-admind
# (patches/sg/0221): SetDynamicTimeZoneInformation / SetTimeZoneInformation
# become sg-admind's "timezone <IANA zone>", SetLocalTime / SetSystemTime its
# "time". Runs the real sg-admind (sg-shell's, SG_ADMIND=) in its unprivileged
# test mode against a spool of its own, with a stand-in timedatectl; and
# checks that without the service (a standard user) the calls are refused.
#   WINE=/opt/wine-sg/bin/wine test/tzset-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
ADMIND="${SG_ADMIND:-$HERE/../../sg-shell/admin/sg-admind}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -f "$ADMIND" ] || { echo "SKIP: no sg-admind (SG_ADMIND=)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tzset.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { [ -n "${LOOP:-}" ] && kill "$LOOP" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/tzset-probe.exe" "$HERE/tzset-probe.c" || { fail "probe did not build"; exit 1; }

# sg-admind's world: a spool, stand-ins, a zoneinfo with the zones asked for
S="$T/spool"; B="$T/bin"; Z="$T/zoneinfo"
mkdir -p "$S/requests" "$S/replies" "$B" "$Z/America" "$Z/Asia"
chmod 700 "$S/requests"
touch "$Z/America/Los_Angeles" "$Z/Asia/Tokyo"
# (sg-admind runs its tools with a minimal environment: the log is named here)
cat > "$B/timedatectl" <<TEOF
#!/bin/sh
echo "timedatectl \$*" >> "$T/calls"
[ "\$1" = show ] && echo no
exit 0
TEOF
chmod +x "$B/timedatectl"
: > "$T/calls"
( export SG_ADMIN_TEST=1 SG_ADMIN_SPOOL="$S" SG_ADMIN_PATH="$B" SG_ADMIN_ZONEINFO="$Z" \
         SG_ADMIN_TIMEZONE="$T/timezone" SG_ADMIN_SYSTEM_UID="$(id -u)"
  while :; do ls "$S/requests"/*.req >/dev/null 2>&1 && python3 "$ADMIND" 2>>"$T/admind.log"; sleep 0.2; done ) &
LOOP=$!

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(SG_ADMIN_SPOOL="$S" timeout -s KILL 120 "$WINE" "$T/tzset-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'; sed 's/^/      /' "$T/calls"
expect() { if printf '%s\n' "$out" | grep -qx "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" || echo none))"; fi; }
called() { if grep -qx "$1" "$T/calls"; then pass "$2"; else fail "$2"; fi; }
expect "dynamic=1,0" "SetDynamicTimeZoneInformation succeeds"
called "timedatectl set-timezone America/Los_Angeles" "  and sets the zone its key names (Pacific Standard Time: America/Los_Angeles)"
expect "standard=1,0" "SetTimeZoneInformation by a zone's standard name succeeds"
called "timedatectl set-timezone Asia/Tokyo" "  and sets that zone (Asia/Tokyo)"
expect "unknown=0,87" "an unknown zone key is ERROR_INVALID_PARAMETER"
expect "local=1,0" "SetLocalTime succeeds"
called "timedatectl set-time 2031-05-06 07:08:09" "  and sets the clock to that local time"
# no service to ask (as for a standard user, who cannot write the spool)
out=$(SG_ADMIN_SPOOL="$T/nowhere" timeout -s KILL 120 "$WINE" "$T/tzset-probe.exe" 2>/dev/null | tr -d '\r')
expect "dynamic=0,1314" "without the service: ERROR_PRIVILEGE_NOT_HELD"
expect "local=0,1314" "  for the clock too"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
