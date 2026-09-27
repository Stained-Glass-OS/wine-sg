#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 0438: shutdown.exe (a stub in Wine). Scripts and installers
# run "shutdown /s /t 0", "shutdown /r", "shutdown /l"; the Ctrl+Alt+Del
# screen's Sign out uses /l. SG_POWERCTL names a stand-in (as for 0241) that
# records what the system was asked.
#   WINE=... test/shutdownexe-gate.sh
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
W=$(mktemp -d /var/tmp/shutdownexe-gate.XXXXXX)
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
cleanup() { set +e; WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null; sleep 1; rm -rf "$W"; }
trap cleanup EXIT
printf '#!/bin/sh\necho "ASKED $*" >> "%s/calls"\n' "$W" > "$W/powerctl"; chmod 755 "$W/powerctl"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" SG_POWERCTL="$W/powerctl"
unset DISPLAY
"$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

run() { # args -> "rc=N" and what the system was asked
    : > "$W/calls"
    "$WINE" shutdown "$@" > "$W/out" 2>&1; rc=$?
    sleep 3
    echo "rc=$rc $(tr '\n' ' ' < "$W/calls")"
}
r=$(run /s /t 0);  [[ "$r" == "rc=0 ASKED shutdown poweroff " ]] && pass "/s /t 0 shuts down" || fail "/s /t 0: $r"
r=$(run -r -t 0);  [[ "$r" == "rc=0 ASKED shutdown reboot " ]] && pass "-r -t 0 restarts (either switch prefix)" || fail "-r: $r"
r=$(run /p);       [[ "$r" == "rc=0 ASKED shutdown poweroff " ]] && pass "/p turns it off at once" || fail "/p: $r"
r=$(run /s /t 2 /c "Maintenance"); [[ "$r" == "rc=0 ASKED shutdown poweroff " ]] && grep -q Maintenance "$W/out" \
    && pass "/t waits, /c shows the comment" || fail "/t /c: $r"
r=$(run /l);       [[ "$r" == "rc=0 " ]] && pass "/l signs out only: the PC is not asked anything" || fail "/l: $r"
r=$(run /a);       [[ "$r" == "rc=92 " ]] && pass "/a with nothing pending: ERROR_NO_SHUTDOWN_IN_PROGRESS (1116; 92 as a Unix exit status)" || fail "/a: $r"
r=$(run /bogus);   [[ "$r" == "rc=87 " ]] && pass "an unknown switch is refused" || fail "/bogus: $r"
r=$(run /?);       [[ "$r" == "rc=0 " ]] && grep -q '^Usage: shutdown' "$W/out" && pass "/? prints the usage" || fail "/?: $r"

echo "shutdownexe-gate: $fails failure(s)"
[ "$fails" = 0 ]
