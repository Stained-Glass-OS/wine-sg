#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# InitiateSystemShutdownEx, InitiateShutdown, AbortSystemShutdown and
# shutdown /a (patches/sg/1618), 64- and 32-bit: test/sysshutdown-probe.c.
# They said yes and did nothing. Every shutdown asked for here is aborted;
# SG_POWERCTL is a stand-in that records power requests, and the gate fails
# if any was made.
#
#   WINE=/opt/wine-sg/bin/wine test/sysshutdown-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_SHUTDOWN (advapi32/advapi.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-sysshutdown.XXXXXX)
printf '#!/bin/sh\necho "$@" >> "%s/power-calls"\n' "$T" > "$T/powerctl"; chmod +x "$T/powerctl"
export SG_POWERCTL="$T/powerctl" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/sysshutdown-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/sysshutdown-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
# shutdown.exe: a delayed shutdown is pending and "shutdown /a" aborts it
"$WINE" shutdown /s /t 600 >/dev/null 2>&1 &
sleep 4
"$WINE" shutdown /a >/dev/null 2>&1; a1=$?
sleep 1
"$WINE" shutdown /a >/dev/null 2>&1; a2=$?
if [ "$a1" = 0 ] && [ "$a2" = 92 ]; then echo "PASS  shutdown /a aborts a pending shutdown, then has nothing to abort"
else echo "FAIL  shutdown /a: $a1 then $a2 (want 0, then 1116 & 255 = 92)"; RC=1; fi
sleep 2
[ -s "$T/power-calls" ] && { echo "FAIL  a power request was made: $(cat "$T/power-calls")"; RC=1; }
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
