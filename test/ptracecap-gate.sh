#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The SYSTEM account's own processes (patches/sg/0624). An elevated program
# runs as SYSTEM with SYSTEM's group, the machine's wineserver with the
# prefix's: the kernel lets a process trace only those whose groups match,
# so the server could not read or write an elevated program's memory
# (ReadProcessMemory, a debugger: access denied; MediaMonkey read its
# parent's PEB as zeros and crashed with runtime error 216, and its
# installer hung on the crash). The server holds CAP_SYS_PTRACE as a
# permitted capability only and makes it effective for one access to a
# process of its own account.
#
# Here: a shared prefix whose server runs as this user with the prefix's
# group and CAP_SYS_PTRACE permitted (sudo + setpriv, as
# sg-wineserver.service gives it), and a program of this user's with this
# user's own group reading and writing its child's memory. Mutant: the same
# server without the capability -- access denied.
#   WINE=... test/ptracecap-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
SG_GROUP=${SG_GROUP:-sgconfgrp}
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v setpriv >/dev/null && sudo -n true 2>/dev/null || { echo "SKIP: needs setpriv and sudo -n"; exit 77; }
G=$(getent group "$SG_GROUP" | cut -d: -f3)
[ -n "$G" ] && id -G | tr ' ' '\n' | grep -qx "$G" && [ "$G" != "$(id -g)" ] ||
    { echo "SKIP: this user must be in $SG_GROUP (not its primary group)"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-ptracecap.XXXXXX)
chmod 755 "$T"
PFX=$T/prefix
export WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sleep 1; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/ptracecap-probe.exe" "$HERE/ptracecap-probe.c" || { fail "probe did not build"; exit 1; }
mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"

server() { # with|without the capability
    "$WINESERVER" -k 2>/dev/null; sleep 1
    if [ "$1" = with ]; then
        sudo -n setpriv --reuid="$(id -u)" --regid="$G" --init-groups --inh-caps=+sys_ptrace --ambient-caps=+sys_ptrace \
            env WINEPREFIX="$PFX" HOME="$HOME" "$WINESERVER" -p
    else
        sudo -n setpriv --reuid="$(id -u)" --regid="$G" --init-groups env WINEPREFIX="$PFX" HOME="$HOME" "$WINESERVER" -p
    fi
    sleep 1
}
server with
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null
cp "$T/ptracecap-probe.exe" "$PFX/drive_c/"
probe() { timeout 60 "$WINE" 'C:\ptracecap-probe.exe' 2>/dev/null | tr -d '\r'; }
sp=""
for p in $(pgrep -u "$(id -u)" -x wineserver); do
    sudo -n cat "/proc/$p/environ" 2>/dev/null | tr '\0' '\n' | grep -qx "WINEPREFIX=$PFX" && sp=$p
done
case "$(grep -E '^(CapPrm|CapEff)' "/proc/${sp:-0}/status" 2>/dev/null | tr '\n' ' ')" in
    *"CapPrm:	0000000000080000"*"CapEff:	0000000000000000"*) pass "the server holds CAP_SYS_PTRACE, permitted only" ;;
    *) fail "server capabilities: $(grep -E '^Cap(Prm|Eff)' "/proc/${sp:-0}/status" 2>/dev/null | tr '\n' ' ')" ;;
esac
out=$(probe)
echo "$out" | grep -qx 'READ ok 0 sg-ptracecap-original' && pass "a process of another group: its memory is read" || fail "read: $out"
echo "$out" | grep -qx 'WRITE ok 0 sg-ptracecap-written' && pass "and written" || fail "write: $out"
# teeth: the same without the capability
server without
out=$(probe)
echo "$out" | grep -q '^READ failed 5' && pass "without the capability, access denied (the gate has teeth)" || fail "without: $out"
exit $RC
