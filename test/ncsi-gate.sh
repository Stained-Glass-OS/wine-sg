#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Network Connectivity Status Indicator's active probe (patches/sg/9661): nsiproxy probes each
# address family that has a default route as Windows' NCSI does -- GET ActiveWebProbeHost/
# ActiveWebProbePath must answer ActiveWebProbeContent (NlaSvc\Parameters\Internet) -- and the
# Network List Manager reaches the internet only when it did. A default route alone was "the
# internet": a hotel network before sign-in or a router with its line down showed "Internet access"
# in every program. In a private network namespace: a dummy interface with a default route, the
# probe page (test/ncsi-server.py) on 127.0.0.1:80 (ActiveWebProbeHost 127.0.0.1), and
# test/ncsi-probe.c holding one manager through: the page answers (internet), a captive portal's
# redirect (local network only), the page again, nothing answering, probing turned off (the route
# decides again) -- its connectivity and its ConnectivityChanged events must follow each.
#   WINE=... test/ncsi-gate.sh        (needs sudo -n, unshare, setpriv, python3)
#   mutants: SG_MUTANT_NO_NCSI (nsiproxy.sys/tcpipreg.c), SG_MUTANT_NLM_IGNORE_NCSI (netprofm/list.c),
#            SG_MUTANT_NCSI_ALWAYS_INTERNET (nsiproxy.sys/ncsi.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" unshare setpriv ip python3; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
W=$(mktemp -d /var/tmp/sg-ncsi.XXXXXX) || exit 1
chmod 777 "$W"
cleanup() { sudo -n rm -rf "$W"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$W/probe.exe" "$HERE/ncsi-probe.c" -lole32 -loleaut32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
cp "$HERE/ncsi-server.py" "$W/server.py"
cat > "$W/inner.sh" <<'INNER'
set -u
W=$1; WINE=$2; WINESERVER=$3; U=$4; G=$5; N=$6; H=$7
ip link set lo up
ip link add sgeth0 type dummy && ip addr add 10.55.0.5/24 dev sgeth0 && ip link set sgeth0 up || { echo "FAIL  no dummy interface"; exit 1; }
ip route add default via 10.55.0.1 dev sgeth0
echo ok > "$W/mode"
python3 "$W/server.py" 127.0.0.1 "$W/mode" "$W/requests" & SP=$!
me() { setpriv --reuid="$U" --regid="$G" --init-groups -- env USER="$N" LOGNAME="$N" HOME="$H" DISPLAY= "$@"; }
export WINEPREFIX="$W/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
me timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
K='HKLM\System\CurrentControlSet\Services\NlaSvc\Parameters\Internet'
me "$WINE" reg add "$K" /v ActiveWebProbeHost /d 127.0.0.1 /f >/dev/null 2>&1
# active probing is off by default until the probe host is decided (main, 2026-11-28): on for the gate
me "$WINE" reg add "$K" /v EnableActiveProbing /t REG_DWORD /d 1 /f >/dev/null 2>&1
me "$WINESERVER" -k; me "$WINESERVER" -w
cp "$W/probe.exe" "$WINEPREFIX/drive_c/"
poke() { ip addr add 10.55.0.$1/24 dev sgeth0; }   # a network change: nsiproxy probes again
wait_ready() { i=0; while [ ! -e "$W/ready-$1" ] && [ $i -lt 900 ]; do sleep 0.1; i=$((i + 1)); done; }
(
    wait_ready 1; poke 11; touch "$W/go-1"
    wait_ready 2; echo portal > "$W/mode"; poke 12; touch "$W/go-2"
    wait_ready 3; echo ok > "$W/mode"; poke 13; touch "$W/go-3"
    wait_ready 4; echo down > "$W/mode"; kill $SP; poke 14; touch "$W/go-4"
    wait_ready 5; me "$WINE" reg add "$K" /v EnableActiveProbing /t REG_DWORD /d 0 /f >/dev/null 2>&1; poke 15; touch "$W/go-5"
    wait_ready 6; touch "$W/go-6"
) &
(cd "$WINEPREFIX/drive_c" && me timeout -s KILL 360 "$WINE" 'C:\probe.exe' "Z:$W" 2>/dev/null </dev/null) | tr -d '\r'
me "$WINE" reg query "$K" 2>/dev/null | tr -d '\r' | grep -i SgActive | sed 's/^/      /'
me "$WINESERVER" -k 2>/dev/null
kill $SP 2>/dev/null
sed 's/^/      probe: /' "$W/requests" 2>/dev/null
grep -q 'GET /connecttest.txt HTTP/1.1 Host: 127.0.0.1' "$W/requests" && echo "PASS  the probe asked for ActiveWebProbePath on ActiveWebProbeHost" ||
    echo "FAIL  no probe request seen"
INNER
out=$(sudo -n -E unshare -n -m --propagation private -- sh "$W/inner.sh" "$W" "$WINE" "$WINESERVER" "$(id -u)" "$(id -g)" "$(id -un)" "$HOME")
printf '%s\n' "$out"
if printf '%s\n' "$out" | grep -q '^FAIL' || ! printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then echo "GATE: FAIL"; exit 1; fi
echo "GATE: PASS"
