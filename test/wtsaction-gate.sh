#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wtsapi32's session actions (patches/sg/2412), 64- and 32-bit, under Xvfb:
# test/wtsaction-probe.c. WTSSendMessage shows a box in the caller's session and
# honours bWait and the timeout; logging off, disconnecting or messaging another
# session needs an administrator; WTSShutdownSystem needs the shutdown privilege
# and runs the system's power helper (a stand-in here, SG_POWERCTL) with
# reboot or poweroff; WTSLogoffSession signs out without calling it.
#
#   WINE=/opt/wine-sg/bin/wine test/wtsaction-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_WTSACT_{NOADMIN,NOPRIV,NOTIMER,ASYNCWAIT,MAP} (wtsapi32/wtsapi32.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for need in Xvfb xdpyinfo x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $need >/dev/null || { echo "SKIP: $need not installed"; exit 77; }
done
DPY="${WTSACTION_DPY:-231}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
RC=0
XP=""
T=$(mktemp -d /var/tmp/sg-wtsaction.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/wtsaction-probe.c" -lwtsapi32 -luser32 -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
printf '#!/bin/sh\necho "$@" >> "%s/power.log"\n' "$T" > "$T/powerctl"
chmod +x "$T/powerctl"
export SG_POWERCTL="$T/powerctl"
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$WINEPREFIX"
i=0; while ! DISPLAY=":$DPY" xdpyinfo >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

run() { # arch args...
    arch=$1; shift
    out=$(cd "$T" && timeout -s KILL 120 env DISPLAY=":$DPY" "$WINE" "$T/probe-$arch.exe" "$@" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
    "$WINESERVER" -w
    sleep 1
}
power_is() { # expected last line ("" = no call)
    got=$(tail -1 "$T/power.log" 2>/dev/null)
    if [ "$got" = "$1" ]; then echo "      PASS  power helper call: '${1:-none}'"
    else echo "      FAIL  power helper call: wanted '${1:-none}', got '$got'"; RC=1; fi
}

for a in x86_64 i686; do
    echo "== $a"
    rm -f "$T/power.log"
    run $a
    echo "-- sign out"
    run $a logoff
    power_is ""
    echo "-- shutdown (power off)"
    run $a shutdown 8
    power_is "shutdown poweroff"
    echo "-- reboot"
    run $a shutdown 4
    power_is "shutdown reboot"
done
echo "== more flags"
run x86_64 shutdown 16
power_is "shutdown reboot"
run x86_64 shutdown 2
power_is "shutdown poweroff"
rm -f "$T/power.log"
run x86_64 shutdown 1
power_is ""
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
