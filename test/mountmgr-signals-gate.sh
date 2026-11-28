#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mountmgr hears only UDisks2's signals (patches/sg/9062).
#
# Its PropertiesChanged rule named no sender: every winedevice.exe got every
# PropertiesChanged on the system bus, systemd's for each unit started and
# stopped. A winedevice.exe that stopped reading (hung, or orphaned when its
# wineserver went) kept them queued until the bus stopped reading systemd:
# systemctl, systemd-run and logins over D-Bus timed out (2026-11-27).
#
# On a private bus of the gate's own (DBUS_SYSTEM_BUS_ADDRESS; never the
# machine's): a fake UDisks2 (empty GetManagedObjects, so mountmgr keeps its
# UDisks2 rules), Wine's services started; the winedevice.exe stopped
# (SIGSTOP), then 4000 PropertiesChanged from another name on a systemd unit's
# path: none queued for the winedevice.exe; then 4000 from UDisks2 on a block
# device's: queued (UDisks2's are still heard).
#   WINE=/opt/wine-sg/bin/wine test/mountmgr-signals-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v dbus-daemon >/dev/null || { echo "SKIP: no dbus-daemon"; exit 77; }
python3 -c 'import dbus, gi' 2>/dev/null || { echo "SKIP: python3-dbus / python3-gi missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sg_gate_display || exit $?

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-mmsig.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}" WINESERVER
BUSPID=""; UDPID=""; D=""
cleanup() {
    for p in $D; do kill -CONT "$p" 2>/dev/null; done
    "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"
    [ -n "$UDPID" ] && kill "$UDPID" 2>/dev/null
    [ -n "$BUSPID" ] && kill "$BUSPID" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

cat > "$T/bus.conf" <<EOC
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:path=$T/bus</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOC
dbus-daemon --config-file="$T/bus.conf" --nofork --nopidfile >/dev/null 2>&1 &
BUSPID=$!
export DBUS_SYSTEM_BUS_ADDRESS="unix:path=$T/bus"
for _ in $(seq 50); do [ -S "$T/bus" ] && break; python3 -c 'import time; time.sleep(0.1)'; done
[ -S "$T/bus" ] || { fail "the private bus did not start"; exit 1; }
python3 "$HERE/mountmgr-signals-probe.py" udisks "$T/flood" 4000 &
UDPID=$!
python3 -c 'import time; time.sleep(1)'

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
# a program that keeps the prefix's services up while the gate looks
timeout -s KILL 200 "$WINE" timeout.exe /t 180 /nobreak >/dev/null 2>&1 &
# winedevice.exe of this prefix only
devs() {
    for d in /proc/[0-9]*; do
        [ -O "$d" ] || continue
        case "$(cat "$d/comm" 2>/dev/null)" in winedevice.exe) ;; *) continue ;; esac
        [ "$({ tr '\0' '\n' < "$d/environ"; } 2>/dev/null | sed -n 's/^WINEPREFIX=//p')" = "$WINEPREFIX" ] && echo "${d#/proc/}"
    done
}
# the services stay up after wineboot -i; wait for a winedevice.exe on the bus
for _ in $(seq 100); do
    D=$(devs | tr '\n' ' ')
    [ -n "$D" ] && python3 "$HERE/mountmgr-signals-probe.py" has $D && break
    python3 -c 'import time; time.sleep(0.2)'
done
[ -n "$D" ] || { fail "no winedevice.exe in the prefix"; exit 1; }
python3 -c 'import time; time.sleep(2)'
for p in $D; do kill -STOP "$p"; done
# the kernel's socket buffer takes the first few hundred unread; the bus
# queues the rest, so 4000 of each
python3 "$HERE/mountmgr-signals-probe.py" flood 4000
python3 -c 'import time; time.sleep(1)'
q1=$(python3 "$HERE/mountmgr-signals-probe.py" stats $D)
touch "$T/flood"
for _ in $(seq 100); do [ -e "$T/flood.done" ] && break; python3 -c 'import time; time.sleep(0.1)'; done
python3 -c 'import time; time.sleep(1)'
q2=$(python3 "$HERE/mountmgr-signals-probe.py" stats $D)
echo "  winedevice.exe ($D): $q1 queued after 4000 of another name's, $q2 after 4000 more of UDisks2's"
[ "$q1" -lt 100 ] && pass "a stopped winedevice.exe does not get another name's PropertiesChanged ($q1 queued of 4000)" \
    || fail "a stopped winedevice.exe gets every PropertiesChanged on the bus: $q1 queued of 4000 another name's"
[ $((q2 - q1)) -gt 1000 ] && pass "UDisks2's own still reach it ($((q2 - q1)) more queued)" \
    || fail "UDisks2's PropertiesChanged no longer reach mountmgr ($((q2 - q1)) more queued of 4000)"
for p in $D; do kill -CONT "$p"; done
exit $RC
