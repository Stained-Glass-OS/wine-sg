#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wlanapi: WlanGetProfileList, WlanConnect, WlanDisconnect, WlanDeleteProfile, WlanReasonCodeToString
# (patches/sg/8017; test/nocode3-probe.c). Spec stubs before. Against a fake sg-netctl (SG_NETCTL): one
# adapter, saved networks MyNet and Cafe; the commands it is given are logged and checked here -- the key of
# a temporary profile reaches it on standard input, never on its command line.
#   WINE=/opt/wine-sg/bin/wine test/nocode3-gate.sh
# Mutants: SG_MUTANT_NO_WLAN_CONNECT, SG_MUTANT_NO_WLAN_PROFILES (wlanapi/main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-nocode3.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/nocode3-probe.c" -lwlanapi 2>"$T/cc.log" ||
    { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
cat > "$T/netctl" <<EOS
#!/bin/sh
echo "ARGS \$*" >> "$T/netctl.log"
case "\$*" in *--password-stdin*) IFS= read -r key; echo "STDIN \$key" >> "$T/netctl.log" ;; esac
case "\$1 \$2" in
    "adapters ") printf 'ADAPTER wlan0\nTYPE wifi\nSTATE connected\nEND\nOK\n' ;;
    "wifi saved") printf 'SAVED 1111-aaaa\tyes\t4d794e6574\tMyNet\nSAVED 2222-bbbb\tno\t43616665\tCafe\nOK\n' ;;
    "wifi connect")
        case "\$*" in *43616665*) printf 'ERROR auth the network security key is not correct\n' ;;
                       *) printf 'CONNECTED wlan0\nOK\n' ;; esac ;;
    "wifi disconnect") printf 'DISCONNECTED wlan0\nOK\n' ;;
    "wifi forget") printf 'FORGOTTEN 1\nOK\n' ;;
    *) printf 'ERROR unsupported no\n' ;;
esac
EOS
chmod +x "$T/netctl"
: > "$T/netctl.log"
export SG_NETCTL="$T/netctl"
mkdir -p "$WINEPREFIX"
unset WAYLAND_DISPLAY DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
timeout -s KILL 120 "$WINESERVER" -w
cd "$T" || exit 1
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" || RC=1
L="$T/netctl.log"
grep -q '^ARGS wifi connect --ssid-hex 4d794e6574 --device wlan0$' "$L" &&
    pass "a saved profile: wifi connect by its SSID on the adapter, its saved key" || fail "a saved profile: $(grep -m1 4d794e6574 "$L")"
grep -q '^ARGS wifi connect --ssid-hex 4775657374202620436f --device wlan0 --security wpa-psk --no-autoconnect --password-stdin$' "$L" &&
    pass "a temporary profile: its SSID, WPA-PSK, not kept, the key on standard input" || fail "a temporary profile: $(grep -m1 477565737420 "$L")"
grep -qx 'STDIN s3cret pass' "$L" && pass "the key reached sg-netctl's standard input" || fail "the key did not reach standard input"
grep '^ARGS' "$L" | grep -q 's3cret' && fail "the key is on a command line" || pass "the key is on no command line"
grep -q '^ARGS wifi connect --ssid-hex 4f70656e --device wlan0 --security open --no-autoconnect$' "$L" &&
    pass "an open network by its SSID" || fail "an open network: $(grep -m1 4f70656e "$L")"
grep -q '^ARGS wifi disconnect wlan0$' "$L" && pass "WlanDisconnect: wifi disconnect wlan0" || fail "no wifi disconnect"
grep -q '^ARGS wifi forget --ssid-hex 43616665$' "$L" && pass "WlanDeleteProfile: wifi forget Cafe" || fail "no wifi forget"
[ $RC = 0 ] && echo "nocode3-gate: all passed"
exit $RC
