#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Caption settings and DHCP client notifications (patches/sg/1700), in a
# user and network namespace of its own (unshare -rn) with a dummy
# interface d0: test/capsdhcp-probe.c reads ClosedCaptionProperties with
# and without the user's settings (the getters always said Default and the
# computed colours were E_NOTIMPL), and registers for DHCP parameter
# changes (DhcpRegisterParamChange was an "@ stub"); this script adds an
# address to d0 when the probe is ready.
#
#   WINE=/opt/wine-sg/bin/wine test/capsdhcp-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_CAPTIONS_DEFAULT (windows.media/captions.c),
# SG_MUTANT_NO_PARAM_CHANGE (dhcpcsvc/dhcpcsvc.c).
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
T=$(mktemp -d /var/tmp/sg-capsdhcp.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER WINE T
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/capsdhcp-probe.exe" "$HERE/capsdhcp-probe.c" -liphlpapi \
    || { echo "FAIL  probe did not build"; exit 1; }
cat > "$T/ns.sh" <<'EOS'
#!/bin/sh
ip link set lo up
ip link add d0 type dummy && ip link set d0 up || { echo "FAIL  no dummy interface"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/capsdhcp-probe.exe" "$WINEPREFIX/drive_c/"
rm -f "$T/ready"
( i=0; while [ ! -e "$T/ready" ] && [ $i -lt 600 ]; do sleep 0.2; i=$((i + 1)); done; sleep 1
  ip addr add 10.9.9.2/24 dev d0 ) &
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 env DISPLAY= "$WINE" capsdhcp-probe.exe "Z:$T/ready" 2>/dev/null </dev/null | tr -d '\r'
"$WINESERVER" -k 2>/dev/null
EOS
chmod +x "$T/ns.sh"
out=$(unshare -rn "$T/ns.sh")
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && exit 0
exit 1
