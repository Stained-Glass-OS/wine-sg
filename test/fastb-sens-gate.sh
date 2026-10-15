#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# sensapi IsDestinationReachable, connect IsInternetConnected (patch 3741)
#   WINE=/opt/wine-sg/bin/wine test/fastb-sens-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${FASTBMS_DPY:-437}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-fastb-sens.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
for inc in "" "-I$(dirname "$WINE")/include" "-I$HERE/../../obj/include" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O2 -mwindows $inc -o "$T/fastb-sens-probe.exe" "$HERE/fastb-sens-probe.c" \
        -lws2_32 -liphlpapi 2>/dev/null && { built=1; break; }
done
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file.
# Three runs: on the host's own network, then in a network namespace with no route and in one with a
# default route (unshare -rn needs no real root), so the outcome does not depend on the machine.
run() { # LABEL PREFIX... : run the probe, collect output
    label=$1; shift
    # each run gets its own server directory, so a namespace run never talks to the host-network server
    mkdir -p "$T/xrd-$label" && chmod 700 "$T/xrd-$label"
    cd "$T" && XDG_RUNTIME_DIR="$T/xrd-$label" timeout -s KILL 120 "$@" "$WINE" "$T/fastb-sens-probe.exe" $MODE >"$T/out.raw" 2>"$T/err.$label" </dev/null
    tr -d '\r' <"$T/out.raw" >"$T/out.$label"
    [ -n "${GATE_DEBUG_ERR:-}" ] && sed "s/^/[$label err] /" "$T/err.$label" | head -30
    sed "s/^/[$label] /" "$T/out.$label"
}
export WINE
MODE=host; run host env
NS=0
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
if command -v unshare >/dev/null && command -v ip >/dev/null && unshare -rn true 2>/dev/null; then
    NS=1
    # the prefix belongs to the calling user, who is root inside the namespace; a wineserver started in one
    # network namespace must not serve the next (its network is the one it was started in), so each is stopped
    MODE=offline; run offline unshare -rn sh -c 'ip link set lo up && "$WINE" wineboot -u >/dev/null 2>&1 && { "$@"; rc=$?; "$WINESERVER" -k; exit $rc; }' sh
    MODE=online; run online unshare -rn sh -c 'ip link set lo up && ip link add d0 type dummy && ip addr add 192.0.2.2/24 dev d0 && ip link set d0 up && ip route add default via 192.0.2.1 dev d0 && "$WINE" wineboot -u >/dev/null 2>&1 && { "$@"; rc=$?; "$WINESERVER" -k; exit $rc; }' sh
else
    echo "note: no usable user+network namespace: only the host run is made"
fi
cat "$T"/out.host "$T"/out.offline "$T"/out.online >"$T/out" 2>/dev/null
if [ "$NS" = 1 ]; then
    for m in offline online; do grep -qx 'RESULT: PASS' "$T/out.$m" || echo "FAIL the $m namespace run did not pass"; done
fi
bad=$(grep -c '^FAIL' "$T/out")
pass=$(grep -c '^RESULT: PASS' "$T/out")
want=1; [ "$NS" = 1 ] && want=3
[ "$bad" = 0 ] && [ "$pass" = "$want" ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL (pass runs $pass of $want, $bad FAIL lines)"
exit 1
