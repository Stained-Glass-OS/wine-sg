#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A metered network's cost, as Windows programs are told it (patches/sg/8425): "Set as metered
# connection" (Settings > Network; NetworkManager's connection.metered, through sg-netctl's
# "cost") makes the Network List Manager's cost Fixed -- INetworkCostManager::GetCost and the
# internet connection's INetworkConnectionCost::GetCost -- as on Windows; not metered: Unrestricted;
# NetworkManager's own guess of metered (a hotspot, mobile broadband): Fixed; unknown: the
# medium's (DefaultMediaCost, Unrestricted by default). In a user and network namespace of its own
# (unshare -rn): a dummy interface with a known address, a default route, and a stand-in
# sg-netctl (SG_NETCTL) answering "cost" from a file the probe (test/nlmmetered-probe.c) rewrites.
#   WINE=/opt/wine-sg/bin/wine test/nlmmetered-gate.sh
# Mutant: SG_MUTANT_NLM_NO_METERED (dlls/netprofm/list.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -rn true 2>/dev/null || { echo "SKIP: no unprivileged network namespaces"; exit 77; }
command -v ip >/dev/null || { echo "SKIP: needs ip (iproute2)"; exit 77; }
T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-nlmmetered.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;${WINEDLLOVERRIDES:-}" WINESERVER WINE T
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/nlmmetered-probe.c" -lole32 -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
cat > "$T/netctl" <<'EOS'
#!/bin/sh
case "$1" in
cost) echo "COST d0 02:00:5e:10:00:01 $(cat "$SG_TEST_METERED")"; echo OK ;;
*) echo "ERROR invalid stand-in knows only cost"; exit 2 ;;
esac
EOS
chmod +x "$T/netctl"
printf yes > "$T/metered"
export SG_NETCTL="$T/netctl" SG_TEST_METERED="$T/metered"
cat > "$T/ns.sh" <<'EOS'
#!/bin/sh
ip link set lo up
ip link add d0 address 02:00:5e:10:00:01 type dummy && ip addr add 10.9.9.2/24 dev d0 && ip link set d0 up &&
    ip route add default via 10.9.9.1 || { echo "FAIL  no dummy interface"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 env DISPLAY= "$WINE" probe.exe "Z:$SG_TEST_METERED" no guess-yes unknown 2>/dev/null </dev/null | tr -d '\r'
"$WINESERVER" -k 2>/dev/null
EOS
chmod +x "$T/ns.sh"
out=$(unshare -rn "$T/ns.sh")
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
# NLM_CONNECTION_COST_UNRESTRICTED 1, _FIXED 2
set -- $(printf '%s\n' "$out" | sed -n 's/^COST //p' | tr '\n' ' ')
chk() { if [ "${1:-}" = "$3" ] && [ "${2:-}" = "$3" ]; then printf 'PASS  %s\n' "$4"; else printf 'FAIL  %s (manager %s, connection %s, want %s)\n' "$4" "${1:-?}" "${2:-?}" "$3"; RC=1; fi; }
chk "${1:-}" "${2:-}" 2 "a network set as metered: the cost is Fixed (manager and connection)"
chk "${3:-}" "${4:-}" 1 "...set as not metered: Unrestricted (asked again past the cache)"
chk "${5:-}" "${6:-}" 2 "...NetworkManager's guess of metered: Fixed"
chk "${7:-}" "${8:-}" 1 "...unknown: the medium's cost (Unrestricted by default)"
exit $RC
