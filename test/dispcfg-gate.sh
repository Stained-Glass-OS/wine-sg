#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0242: a standard user's programs read the display
# configuration back in a shared (system) prefix.
#
# win32u records the adapters, sources and modes under HKLM\HARDWARE\DEVICEMAP\
# VIDEO and Hardware Profiles\Current\...\Control\Video and reads them back.
# Nobody but an administrator could create those containers, so every signed-in
# user's program logged "Failed to read display config": Settings offered a
# "0 x 0" resolution and nothing else, and DirectDraw's primary surface failed
# (Media Player crashed on any video). wineboot, run as SYSTEM when the machine
# server starts, now creates them; the server makes them user-writable.
#
# This user owns the prefix (SYSTEM there); SG_OTHER (default sgconf, in
# SG_GROUP) is the standard user. Needs passwordless `sudo -u $SG_OTHER` and
# Xvfb. Exit 77 when they are missing. The probe runs on the user's own
# desktop (explorer /desktop=shell,1280x800), as in a session.
#   WINE=... test/dispcfg-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DISP=${DISP:-:173}
W=$(mktemp -d /var/tmp/dispcfg.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null && command -v Xvfb >/dev/null || { echo "SKIP: no mingw/Xvfb"; exit 77; }
XPID=
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    [ -n "$XPID" ] && kill "$XPID" 2>/dev/null
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT
Xvfb "$DISP" -screen 0 1280x800x24 -nolisten tcp -ac >/dev/null 2>&1 &
XPID=$!
sleep 1

"$MINGW" -O2 -o "$W/probe.exe" "$HERE/dispcfg-probe.c" -luser32 || { echo "FAIL build"; exit 1; }
chmod 755 "$W/probe.exe"
mkdir -m 777 "$W/out"
mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=err+system,-all WINEDLLOVERRIDES="mscoree,mshtml=" DISPLAY=$DISP
# The machine: its server and wineboot as the prefix owner (SYSTEM), as
# sg-wineserver does, with no display of its own.
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; DISPLAY= '$WINE' wineboot -i" >/dev/null 2>&1
# The machine server restarted as at boot: its first process -- which runs
# wineboot, and so makes HKLM\HARDWARE -- is on the services' window station
# (sg-services-start), which has no adapters and records no display.
sg "$SG_GROUP" -c "'$WINESERVER' -k" 2>/dev/null; sleep 1
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; DISPLAY= SG_WINSTATION='__wineservice_winstation\\Default' '$WINE' cmd /c exit" >/dev/null 2>&1
# ... and the display containers sg-session's sg_protect_machine_registry
# makes at every boot (wine-sg 0011 gives keys beneath them a writable DACL).
for k in 'HKLM\System\CurrentControlSet\Control\Video' 'HKLM\System\CurrentControlSet\Control\GraphicsDrivers'; do
    sg "$SG_GROUP" -c "umask 002; DISPLAY= '$WINE' reg add '$k' /f" >/dev/null 2>&1
done
chmod -R g+rwX "$PFX" 2>/dev/null

sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=${GATE_DEBUG:-err+system,-all} WINEDLLOVERRIDES="mscoree,mshtml=" \
    DISPLAY="$DISP" HOME=/var/tmp "$WINE" explorer "/desktop=shell,1280x800" "$W/probe.exe" 'Z:'"${W//\//\\}"'\out\probe.txt' > /dev/null 2> "$W/other.err" &
for _ in $(seq 1 60); do [ -s "$W/out/probe.txt" ] && grep -q MODES "$W/out/probe.txt" && break; sleep 1; done
cp "$W/out/probe.txt" "$W/other.out" 2>/dev/null || : > "$W/other.out"
tr -d '\r' < "$W/other.out"
errs=$(grep -c 'Failed to read display config' "$W/other.err")
[ "$errs" = 0 ] && pass "the standard user's program reads the display configuration back" \
    || fail "the standard user's program: $errs x 'Failed to read display config'"
grep -q '^CURRENT 1 1280x800' "$W/other.out" && pass "its current mode is the screen's, 1280x800" \
    || fail "current mode: $(grep CURRENT "$W/other.out")"
n=$(sed -n 's/^MODES //p' "$W/other.out" | tr -d '\r')
[ "${n:-0}" -gt 1 ] && pass "it is offered the screen's modes ($n)" || fail "modes offered: ${n:-none}"
# The rest of HKLM is still the administrators': the standard user cannot
# create a key beside these.
if sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all DISPLAY="$DISP" HOME=/var/tmp \
        "$WINE" reg add 'HKLM\HARDWARE\DEVICEMAP\SgGateOther' /f >/dev/null 2>&1; then
    fail "a standard user could create HKLM\\HARDWARE\\DEVICEMAP\\SgGateOther"
else pass "the rest of HKLM\\HARDWARE\\DEVICEMAP stays administrators-only"; fi
[ -n "${GATE_KEEP:-}" ] && { cp "$W/other.err" "$GATE_KEEP"; }
echo "dispcfg-gate: $fails failure(s)"
exit $((fails > 0))
