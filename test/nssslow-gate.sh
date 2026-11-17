#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The server keeps serving while it asks the account databases about a connecting account (7824).
# On a shared prefix the server asks NSS whether an account may connect and whether it is an
# administrator (sg-admins, 7737). Through winbind (nsswitch.conf "files winbind") an answer can
# take minutes while winbindd waits on a domain controller; asked on the main loop, every Windows
# program of every account waited meanwhile, and the server did not end on SIGTERM.
#   In a user namespace with two users on a shared prefix, the server's lookups made slow (an
#   LD_PRELOAD shim, inside wineserver only: SG_SLOW_NSS=6 seconds per lookup):
#   - the first account's round trips to the server, measured while the second account connects,
#     stay fast (worst under 2 s, the lookups take about 12 s);
#   - the second account's program runs, with its token as before: elevation type Default (not an
#     administrator), its answer computed as before.
#   WINE=/opt/wine-sg/bin/wine test/nssslow-gate.sh   (mutant SG_MUTANT_NSS_ON_MAIN_LOOP, server nssworker.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" gcc setpriv; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
SUB=$(awk -F: -v u="$(id -un)" '$1 == u { print $2; exit }' /etc/subuid)
SUBG=$(awk -F: -v u="$(id -un)" '$1 == u { print $2; exit }' /etc/subgid)
MAP="--map-users=0:$(id -u):1 --map-users=1000:${SUB:-0}:100 --map-groups=0:$(id -g):1 --map-groups=1000:${SUBG:-0}:100"
{ [ -n "$SUB" ] && [ -n "$SUBG" ] && unshare $MAP -m true 2>/dev/null; } || { echo "SKIP: no two-user namespaces"; exit 77; }
T=$(mktemp -d /var/tmp/sg-nssslow.XXXXXX)
chmod 755 "$T"
cleanup() { unshare $MAP rm -rf "$T" 2>/dev/null; rm -rf "$T" 2>/dev/null; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/nssslow-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp gcc -shared -fPIC -O1 -o "$T/nssslow.so" "$HERE/nssslow-shim.c" -ldl || { echo "FAIL  shim did not build"; exit 1; }
chmod 755 "$T/probe.exe" "$T/nssslow.so"
export T WINE WINESERVER
cat > "$T/inner.sh" <<'EOS'
set -u
export WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WAYLAND_DISPLAY=sg-no-wayland
unset DISPLAY
P="$T/shared"; mkdir -p "$P" "$T/h0" "$T/h1" "$T/x0" "$T/x1"
chown 0:1050 "$P"; chmod 2770 "$P"; touch "$P/.sg-system-prefix"
chown 1001:1001 "$T/h1" "$T/x1"; chmod 700 "$T/x0" "$T/x1"
export WINEPREFIX="$P"
HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" "$WINESERVER" -k 2>/dev/null; HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" "$WINESERVER" -w
cp "$T/probe.exe" "$P/drive_c/"; chmod 755 "$P/drive_c/probe.exe"
# the server, its lookups slow
HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" LD_PRELOAD="$T/nssslow.so" SG_SLOW_NSS=6 "$WINESERVER" -p 120 2> "$T/server.err" &
sleep 2
(cd "$P/drive_c" && HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" timeout -s KILL 90 "$WINE" probe.exe ping 22 > "$T/ping.out" 2>/dev/null) &
PING=$!
sleep 4
start=$(date +%s)
who=$(cd "$P/drive_c" && setpriv --reuid=1001 --regid=1001 --groups=1050 env HOME="$T/h1" XDG_RUNTIME_DIR="$T/x1" \
      timeout -s KILL 120 "$WINE" probe.exe who 2>"$T/who.err" | tr -d '\r')
took=$(( $(date +%s) - start ))
wait $PING
ping=$(tr -d '\r' < "$T/ping.out")
HOME="$T/h0" XDG_RUNTIME_DIR="$T/x0" "$WINESERVER" -k 2>/dev/null
echo "      first account, while the second connected: $ping"
echo "      second account ($took s): $who"; [ -n "$who" ] || sed "s/^/        stderr: /" "$T/who.err" | tail -5
rc=0
worst=$(printf '%s\n' "$ping" | sed -n 's/^Worst=\([0-9]*\).*/\1/p')
if [ -n "$worst" ] && [ "$worst" -lt 2000 ]; then echo "PASS  the server answered the first account within ${worst} ms while it asked about the second"
else echo "FAIL  the first account waited ${worst:-?} ms for the server (the lookups took about 12 s)"; rc=1; fi
[ "$took" -ge 6 ] && echo "PASS  the second account's connection waited for its lookups (${took} s)" \
    || { echo "FAIL  the second account did not wait for its lookups (${took} s): were they slow?"; rc=1; }
[ "$who" = "Elevation=1" ] && echo "PASS  the second account's program ran, its token as before: elevation type Default" \
    || { echo "FAIL  the second account: '$who' (wanted Elevation=1)"; rc=1; }
exit $rc
EOS
unshare $MAP -m sh "$T/inner.sh"; RC=$?
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
