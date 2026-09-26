#!/bin/bash
# Gate for wine-sg 0241: ExitWindowsEx shuts down and restarts the PC.
#
# Wine's ExitWindowsEx only ran wineboot --end-session: Start's Shut down and
# Restart (and every program's) ended nothing but the Windows session -- the
# PC stayed on. Now, after starting wineboot, it starts the native helper
# (sg-session's sg-settingsctl shutdown poweroff|reboot, which waits for the
# session's programs to close and asks logind). SG_POWERCTL names a stand-in
# here that records what it was asked, and that it outlives the session's end.
#   WINE=... test/shutdown-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
W=$(mktemp -d /var/tmp/shutdown-gate.XXXXXX)
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
cleanup() { set +e; WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null; sleep 1; rm -rf "$W"; }
trap cleanup EXIT

"$MINGW" -O2 -o "$W/probe.exe" "$HERE/shutdown-probe.c" -luser32 || { echo "FAIL build"; exit 1; }
# The stand-in: what it was asked, then -- a few seconds later, after the
# session has ended -- that it is still alive.
cat > "$W/powerctl" <<EOS
#!/bin/sh
echo "ASKED \$*" >> "$W/calls"
sleep 3
echo "ALIVE \$*" >> "$W/calls"
EOS
chmod 755 "$W/powerctl"

export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" SG_POWERCTL="$W/powerctl"
unset DISPLAY
"$WINE" wineboot -i >/dev/null 2>&1

run() { # probe verb -> the stand-in's lines for it, after it has had time
    : > "$W/calls"
    "$WINE" "$W/probe.exe" "$1" > "$W/$1.out" 2>&1
    sleep 5
    "$WINESERVER" -w 2>/dev/null
    tr -d '\r' < "$W/$1.out"
}
out=$(run shutdown)
grep -q '^EXIT shutdown 1' <<<"$out" && pass "ExitWindowsEx(EWX_SHUTDOWN) succeeds" || fail "EWX_SHUTDOWN: $out"
[ "$(cat "$W/calls")" = "$(printf 'ASKED shutdown poweroff\nALIVE shutdown poweroff')" ] \
    && pass "Shut down asks the system to power off, and the helper outlives the session's end" \
    || fail "Shut down: $(tr '\n' '|' < "$W/calls")"
run reboot >/dev/null
[ "$(head -1 "$W/calls")" = "ASKED shutdown reboot" ] && pass "Restart asks the system to reboot" \
    || fail "Restart: $(tr '\n' '|' < "$W/calls")"
run poweroff >/dev/null
[ "$(head -1 "$W/calls")" = "ASKED shutdown poweroff" ] && pass "EWX_POWEROFF powers off" \
    || fail "EWX_POWEROFF: $(tr '\n' '|' < "$W/calls")"
out=$(run logoff)
grep -q '^EXIT logoff 1' <<<"$out" && [ ! -s "$W/calls" ] && pass "Sign out ends the session only: the PC is not asked anything" \
    || fail "Sign out: $out $(tr '\n' '|' < "$W/calls")"
# A relative or empty SG_POWERCTL is not a Unix path: the default helper is
# used (absent here, so nothing is run and ExitWindowsEx still succeeds).
: > "$W/calls"
out=$(SG_POWERCTL=powerctl "$WINE" "$W/probe.exe" shutdown 2>&1 | tr -d '\r'); sleep 4
grep -q '^EXIT shutdown 1' <<<"$out" && [ ! -s "$W/calls" ] && pass "only an absolute Unix path names a stand-in" \
    || fail "relative SG_POWERCTL: $out $(tr '\n' '|' < "$W/calls")"

echo "shutdown-gate: $fails failure(s)"
exit $((fails > 0))
