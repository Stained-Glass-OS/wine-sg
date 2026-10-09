#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GetBestRoute2 (patches/sg/1690), in a user and network namespace of its
# own (unshare -rn): d0 10.9.9.2/24 with the default route via 10.9.9.1,
# d1 10.8.0.2/16 with 10.8.5.0/24 via 10.8.0.1. test/bestroute-probe.c asks
# for the best route and source to several destinations, on one interface
# and from one source address. It was a stub (ERROR_NOT_SUPPORTED).
#
#   WINE=/opt/wine-sg/bin/wine test/netlist-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_BEST_ROUTE (iphlpapi/iphlpapi_main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -rn true 2>/dev/null || { echo "SKIP: no unprivileged network namespaces"; exit 77; }
command -v ip >/dev/null || { echo "SKIP: needs ip (iproute2)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-bestroute.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER WINE T
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/bestroute-probe.exe" "$HERE/bestroute-probe.c" -liphlpapi -lws2_32 \
    || { echo "FAIL  probe did not build"; exit 1; }
cat > "$T/ns.sh" <<'EOS'
#!/bin/sh
ip link set lo up
ip link add d0 type dummy && ip addr add 10.9.9.2/24 dev d0 && ip link set d0 up || { echo "FAIL  no dummy interface"; exit 1; }
ip link add d1 type dummy && ip addr add 10.8.0.2/16 dev d1 && ip link set d1 up
ip route add default via 10.9.9.1 && ip route add 10.8.5.0/24 via 10.8.0.1
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/bestroute-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 env DISPLAY= "$WINE" bestroute-probe.exe 2>/dev/null </dev/null | tr -d '\r'
"$WINESERVER" -k 2>/dev/null
EOS
chmod +x "$T/ns.sh"
out=$(unshare -rn "$T/ns.sh")
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && exit 0
exit 1
