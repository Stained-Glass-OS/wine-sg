#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0247 and 0248: a sandboxed program's own desktop comes up,
# and a restricted token may read HKLM (0248).
#
# Firefox's sandbox starts each content process on an alternate window station
# and desktop with a restricted, low-integrity token. The first window on that
# desktop starts an explorer for it -- under that token, which may not write
# the registry: win32u's display setup found no GPU and asserted
# (sysparams.c: add_source: Assertion `!list_empty( &gpus )'), the abort left
# that explorer spinning at 100% CPU forever, and the sandboxed process waited
# for its desktop forever -- every Firefox start added spinning explorers until
# threads could not be created. The display setup now gives up quietly when a
# step finds nothing (the desktop reads what the session already recorded).
#
# This user owns the prefix (SYSTEM there); SG_OTHER (default sgconf, in
# SG_GROUP) is the standard user, on its own shell desktop. Needs passwordless
# `sudo -u $SG_OTHER` and Xvfb. Exit 77 when they are missing.
#   WINE=... test/sandboxdesk-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
DISP=${DISP:-:171}
W=$(mktemp -d /var/tmp/sandboxdesk.XXXXXX)
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

"$MINGW" -O2 -o "$W/probe.exe" "$HERE/sandboxdesk-probe.c" -ladvapi32 -luser32 || { echo "FAIL build"; exit 1; }
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

other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=${GATE_DEBUG:-err+system,-all} WINEDLLOVERRIDES="mscoree,mshtml=" \
        DISPLAY="$DISP" HOME=/var/tmp "$WINE" "$@"
}
# the standard user's session: its shell desktop
other explorer "/desktop=shell,1280x800" > /dev/null 2> "$W/shell.err" &
sleep 6
# the sandboxed child, from a program on that desktop
timeout 60 sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=err+system,-all WINEDLLOVERRIDES="mscoree,mshtml=" \
    DISPLAY="$DISP" HOME=/var/tmp "$WINE" explorer "/desktop=shell,1280x800" "$W/probe.exe" 'Z:'"${W//\//\\}"'\out\child.txt' \
    > /dev/null 2> "$W/probe.err" &
for _ in $(seq 1 40); do [ -s "$W/out/child.txt" ] && break; sleep 1; done
sleep 8
tr -d '\r' < "$W/out/child.txt" > "$W/out/child.lf" 2>/dev/null && mv "$W/out/child.lf" "$W/out/child.txt"   # the probe writes CRLF
cat "$W/out/child.txt" 2>/dev/null
grep -q '^CHILD_WINDOW 1' "$W/out/child.txt" 2>/dev/null && pass "the sandboxed process gets a window on its own desktop" \
    || fail "the sandboxed process made no window: $(cat "$W/out/child.txt" 2>/dev/null)"
grep -q '^CHILD_HKLM 0$' "$W/out/child.txt" 2>/dev/null && pass "it may read HKLM (RESTRICTED is granted read, as on Windows)" \
    || fail "the sandboxed process cannot read HKLM: $(grep HKLM "$W/out/child.txt" 2>/dev/null)"
grep -q '^CHILD_MODE 1 1280x800' "$W/out/child.txt" 2>/dev/null && pass "and sees the display (1280x800)" \
    || fail "the sandboxed process's display: $(grep MODE "$W/out/child.txt" 2>/dev/null)"
n=$(cat "$W"/*.err | grep -c 'Assertion')
[ "$n" = 0 ] && pass "no display-setup assertion" || fail "$n x: $(cat "$W"/*.err | grep -m1 Assertion)"
# nothing of this prefix left spinning: explorers other than the shell's
spin=0
for p in $(pgrep -u "$SG_OTHER" -f 'explorer.exe'); do
    sudo -n -u "$SG_OTHER" cat /proc/$p/environ 2>/dev/null | tr '\0' '\n' | grep -qx "WINEPREFIX=$PFX" || continue
    c=$(ps -o pcpu= -p "$p" | tr -d ' ' | cut -d. -f1)
    [ "${c:-0}" -ge 50 ] && { spin=$((spin + 1)); echo "  spinning: $(ps -o pid=,pcpu=,args= -p "$p")"; }
done
[ "$spin" = 0 ] && pass "no explorer left spinning" || fail "$spin explorer(s) spinning at 50%+ CPU"
echo "sandboxdesk-gate: $fails failure(s)"
exit $((fails > 0))
