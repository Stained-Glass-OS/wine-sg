#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Network List Manager (patches/sg/1689), in a user and network
# namespace of its own (unshare -rn): a dummy interface with an address and
# no default route. test/netlist-probe.c reads a network's name,
# description, times and category, sets its name and category (a new
# manager reads them back from the profile), calls the manager through
# IDispatch, enumerates connection points and sinks, sets DefaultMediaCost
# to metered, and waits for ConnectivityChanged to say "IPv4 internet" when
# this script adds a default route; the cost is then fixed. These were
# stubs: E_NOTIMPL, FIXMEs, and no events.
#
#   WINE=/opt/wine-sg/bin/wine test/netlist-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_DISPATCH, SG_MUTANT_NO_EVENTS, SG_MUTANT_NO_MEDIA_COST
# (netprofm/list.c).
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
T=$(mktemp -d /var/tmp/sg-netlist.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER WINE T
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/netlist-probe.exe" "$HERE/netlist-probe.c" -lole32 -loleaut32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
cat > "$T/ns.sh" <<'EOS'
#!/bin/sh
ip link set lo up
ip link add d0 type dummy && ip addr add 10.9.9.2/24 dev d0 && ip link set d0 up || { echo "FAIL  no dummy interface"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/netlist-probe.exe" "$WINEPREFIX/drive_c/"
rm -f "$T/ready"
( i=0; while [ ! -e "$T/ready" ] && [ $i -lt 600 ]; do sleep 0.2; i=$((i + 1)); done; sleep 1
  ip route add default via 10.9.9.1 ) &
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 env DISPLAY= "$WINE" netlist-probe.exe "Z:$T/ready" 2>/dev/null </dev/null | tr -d '\r'
"$WINESERVER" -k 2>/dev/null
EOS
chmod +x "$T/ns.sh"
out=$(unshare -rn "$T/ns.sh")
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && exit 0
exit 1
