#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SetSuspendState through systemd-logind (patches/sg/2414), 64- and 32-bit:
# test/suspend-probe.c against a stand-in logind (test/suspend-logind-standin.c)
# on a private dbus-daemon, which the Wine process reaches through
# DBUS_SYSTEM_BUS_ADDRESS. Suspend or hibernate as asked, and the call
# returns only once the machine is back (the stand-in sends PrepareForSleep
# true, pauses, then false); refusals map to what Windows reports.
#
#   WINE=/opt/wine-sg/bin/wine test/suspend-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_SUSPEND_ALWAYS_TRUE (powrprof/powrprof.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for need in dbus-daemon gcc pkg-config x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $need >/dev/null || { echo "SKIP: $need not installed"; exit 77; }
done
pkg-config --exists dbus-1 || { echo "SKIP: libdbus headers not installed"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-suspend.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
BUS="" ST=""
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$ST" ] && kill "$ST" 2>/dev/null; [ -n "$BUS" ] && kill "$BUS" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/suspend-probe.c" -lpowrprof -ladvapi32 \
        || { echo "FAIL  probe did not build"; exit 1; }
done
gcc -o "$T/standin" "$HERE/suspend-logind-standin.c" $(pkg-config --cflags --libs dbus-1) || { echo "FAIL  stand-in did not build"; exit 1; }
dbus-daemon --session --address="unix:path=$T/bus" --nofork --nopidfile >/dev/null 2>&1 & BUS=$!
i=0; while [ ! -S "$T/bus" ] && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
: > "$T/config"; : > "$T/log"
DBUS_SESSION_BUS_ADDRESS="unix:path=$T/bus" "$T/standin" "$T/config" "$T/log" & ST=$!
i=0; while ! grep -q READY "$T/log" && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# row: name | stand-in config | probe mode | bus address | expected probe output (grep -E) | expected log
row() {
    name=$1; cfg=$2; mode=$3; bus=$4; want=$5; wantlog=$6
    for a in x86_64 i686; do
        printf '%s\n' "$cfg" > "$T/config"; : > "$T/log"
        out=$(cd "$T" && DBUS_SYSTEM_BUS_ADDRESS="$bus" timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe-$a.exe" $mode 2>/dev/null </dev/null | tr -d '\r')
        got=$(tr '\n' ',' < "$T/log" | sed 's/,$//; s/^READY,//')
        if printf '%s\n' "$out" | grep -Eq "$want" && [ "$got" = "$wantlog" ]; then
            echo "PASS  $name ($a): $out | log: $got"
        else
            echo "FAIL  $name ($a): got '$out' log '$got'; wanted '$want' log '$wantlog'"; RC=1
        fi
    done
    "$WINESERVER" -w
}
BUSADDR="unix:path=$T/bus"
row "suspend returns after the machine is back"   ""                     suspend   "$BUSADDR" 'ret=1 err=0 ms=1[1-9][0-9][0-9]$'   "CanSuspend,Suspend false"
row "hibernate asks for hibernation"               ""                     hibernate "$BUSADDR" 'ret=1 err=0 ms=1[1-9][0-9][0-9]$'   "CanHibernate,Hibernate false"
row "hibernation not available"                    "CanHibernate=na"      hibernate "$BUSADDR" 'ret=0 err=50 '                         "CanHibernate"
row "suspend not available"                        "CanSuspend=na"        suspend   "$BUSADDR" 'ret=0 err=50 '                         "CanSuspend"
row "logind says no: access denied"                "CanSuspend=no"        suspend   "$BUSADDR" 'ret=0 err=5 '                          "CanSuspend"
row "challenge is tried and polkit refuses"        "CanSuspend=challenge
Suspend=denied"                                                           suspend   "$BUSADDR" 'ret=0 err=5 '                          "CanSuspend,Suspend false"
row "any other failure: general failure"           "Suspend=fail"         suspend   "$BUSADDR" 'ret=0 err=31 '                         "CanSuspend,Suspend false"
row "no system bus: not supported"                 ""                     suspend   "unix:path=$T/nobus" 'ret=0 err=50 '               ""
row "without the shutdown privilege"               ""                     nopriv    "$BUSADDR" 'ret=0 err=1314 '                       ""
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
