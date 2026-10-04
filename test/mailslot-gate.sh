#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A mailslot can be given to an I/O completion port (patches/sg/0798), as on
# Windows: CreateMailslot made it synchronous and the port was refused --
# Opera's installer CHECKs that and stopped (2026-10-04). Reads without an
# OVERLAPPED still wait (a message there, the read timeout), and an
# overlapped read completes through the port.
#
#   WINE=/opt/wine-sg/bin/wine test/mailslot-gate.sh   (mutant SG_MUTANT_MAILSLOT_SYNC)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-mailslot.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/mailslot-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout 60 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/o"
v() { sed -n "s/^$1 //p" "$T/o" | head -1; }
[ "$(v READ)" = "1 hello" ] && pass "a blocking read gets the message there" || fail "read: '$(v READ)'"
[ "$(v TIMEOUT)" = "0 121 waited" ] && pass "with none, the read waits its timeout and fails with ERROR_SEM_TIMEOUT" || fail "timeout: '$(v TIMEOUT)'"
case "$(v PORT)" in "yes "*) pass "CreateIoCompletionPort takes the mailslot";; *) fail "completion port: '$(v PORT)'";; esac
[ "$(v COMPLETED)" = "1 77 5 world" ] && pass "an overlapped read completes through the port (key 77, 5 bytes)" || fail "completion: '$(v COMPLETED)'"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
