#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A wineserver that dies before it answers a new process's first request (patches/sg/9200).
# The process kept its own copy of the reply pipe's write end until the reply came, so the
# read never saw end-of-file and the process waited for ever (bugs1, 2026-11-22: "wineserver -k"
# during a session start left a process hung). A stand-in server (srvdeath-standin.py) hands
# the process its connection as a server does, takes the init_first_thread request and dies:
# once with the process's reply and wait pipes received, once with them still in the socket.
# The process must end within seconds, with status 1 and a message that says what happened.
#   WINE=/opt/wine-sg/bin/wine test/srvdeath-gate.sh
# Mutant: SG_MUTANT_KEEP_REPLY_WRITER (dlls/ntdll/unix/server.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-srvdeath.XXXXXX)
trap 'rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
# no server is ever started: the stand-in is the only one, and the prefix is never touched
export WINEPREFIX="$T/prefix" WINEDEBUG=-all
mkdir -p "$WINEPREFIX"
SD="python3 -I $HERE/srvdeath-standin.py"
# the protocol version: the process names it when the stand-in offers 0
SRVDEATH_STDERR="$T/err" $SD probe 0 20 "$WINE" cmd.exe /c exit >/dev/null
V=$(sed -n 's/.*version mismatch 0\/\([0-9][0-9]*\)\..*/\1/p' "$T/err" | head -n1)
[ -n "$V" ] || { echo "FAIL  no protocol version from the process:"; cat "$T/err"; exit 1; }
echo "ok   protocol version $V"
RC=0
for mode in read noread; do
    out=$(SRVDEATH_STDERR="$T/err" $SD "$mode" "$V" 20 "$WINE" cmd.exe /c exit)
    set -- $out
    if [ "$1" = EXIT ] && [ "$2" = 1 ] && awk "BEGIN { exit !($3 < 10) }"; then
        echo "ok   $mode: the process ended in $3 s with status 1"
    else
        echo "FAIL $mode: $out (want: ended within 10 s, status 1)"; RC=1
    fi
    if grep -q "wineserver exited before it finished starting this process" "$T/err"; then
        echo "ok   $mode: it said why"
    else
        echo "FAIL $mode: no message; stderr was:"; sed 's/^/     /' "$T/err"; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
