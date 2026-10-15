#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE user32: DdeDisconnect from the server side ends the conversation; the
# client gets XTYP_DISCONNECT.
#
#   WINE=/opt/wine-sg/bin/wine test/user32-ddedisconnect-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_DDE_SERVER_DISCONNECT_FAILS, SG_MUTANT_DDE_CLIENT_NO_DISCONNECT_CB (user32/dde_client.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-user32-ddedisconnect.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/user32-ddedisconnect-probe.c"  &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/user32-ddedisconnect-probe.c"  ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    for dll in user32; do
        if command -v xvfb-run >/dev/null; then X="xvfb-run -a -s -screen\ 0\ 1024x768x24"; else X=""; fi
        out=$(cd "$T" && eval timeout -s KILL 120 $X '"$WINE"' '"$T/$p.exe"' $dll 2>/dev/null </dev/null | tr -d '\r')
        if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
            echo "ok   $p $dll ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
        else
            echo "FAIL $p $dll"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
        fi
    done
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
