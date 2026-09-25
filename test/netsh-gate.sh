#!/bin/sh
# ipconfig and netsh on sg-netctl (patches/sg/0078-0079).
#
# 0078: a native program gets the files it is given as stdin, stdout and
#   stderr, also from a program with no console (stock Wine closed them).
# 0079: ipconfig /release /renew and netsh's interface and wlan commands ask
#   sg-netctl -- here a stand-in that records what it was asked and answers
#   with an Ethernet adapter on DHCP and a Wi-Fi one -- and print Windows'
#   answers; a refusal reads as Windows' elevation message; netsh commands
#   it does not know still succeed quietly, as before.
#
#   WINE=/opt/wine-sg/bin/wine test/netsh-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-netsh.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/nativeio-probe.exe" "$HERE/nativeio-probe.c" || { fail "probe did not build"; exit 1; }
cat > "$T/fake-netctl" <<'EOF'
#!/bin/sh
{ printf 'CALL'; for a in "$@"; do printf ' [%s]' "$a"; done; printf '\n'; } >> "$FAKE_LOG"
if [ -n "${FAKE_DENY:-}" ] && [ "$1" != adapters ]; then echo "ERROR denied changing an adapter's settings needs an administrator"; exit 3; fi
case "$1" in
adapters)
    printf 'ADAPTER eth0\nTYPE ethernet\nSTATE connected\nIPV4-METHOD auto\nIPV4-DNS-AUTO yes\nIPV4-ADDRESS 10.0.2.15/24\nIPV4-GATEWAY 10.0.2.2\nIPV4-DNS 10.0.2.3\nEND\n'
    printf 'ADAPTER wlan0\nTYPE wifi\nSTATE disconnected\nIPV4-METHOD manual\nEND\nOK\n' ;;
wifi)
    case "$2" in
    scan) printf 'WIFI 70\twpa-psk\tno\tyes\t43616665\tCafe\nOK\n' ;;
    saved) printf 'SAVED u-1\tyes\t43616665\tCafe\nOK\n' ;;
    *) echo OK ;;
    esac ;;
*) echo OK ;;
esac
EOF
chmod +x "$T/fake-netctl"
export SG_NETCTL="$T/fake-netctl" FAKE_LOG="$T/calls"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/nativeio-probe.exe" "$WINEPREFIX/drive_c/"

run() { : > "$T/calls"; timeout -s KILL 60 "$WINE" "$@" > "$T/out" 2>/dev/null; code=$?; out=$(tr -d '\r' < "$T/out"); calls=$(cat "$T/calls"); }

# 0078
run C:\\nativeio-probe.exe
case "$out" in *"[got:hello"*err*) pass "a native child writes to the files it was given (stdout and stderr)" ;; *) fail "console parent: $out" ;; esac
run C:\\nativeio-probe.exe detached
case "$out" in *"[got:hello"*err*) pass "and so it does when started with no console, as a GUI program's is" ;; *) fail "detached: $out" ;; esac

# 0079: ipconfig
run ipconfig /renew
[ "$calls" = "$(printf 'CALL [adapters]\nCALL [renew] [eth0]\nCALL [adapters]')" ] || [ "$(printf '%s' "$calls" | grep -c 'renew')" = 1 ] && \
    printf '%s' "$calls" | grep -q 'CALL \[renew\] \[eth0\]' && ! printf '%s' "$calls" | grep -q 'wlan0' \
    && pass "ipconfig /renew renews the adapter on DHCP, and only it" || fail "ipconfig /renew asked: $calls"
run ipconfig /release "eth*"
printf '%s' "$calls" | grep -q 'CALL \[release\] \[eth0\]' && pass "ipconfig /release eth* releases eth0" || fail "ipconfig /release: $calls"

# netsh interface ip
run netsh interface ip set address name="eth0" static 10.0.2.50 255.255.255.0 10.0.2.2
printf '%s' "$calls" | grep -q 'CALL \[ipv4\] \[eth0\] \[static\] \[10.0.2.50/24\] \[--gateway\] \[10.0.2.2\] \[--dns\] \[10.0.2.3\]' \
    && [ $code = 0 ] && pass "set address static: address/prefix, gateway, and the DNS it had" || fail "set address static ($code): $calls"
run netsh interface ipv4 set address "eth0" dhcp
printf '%s' "$calls" | grep -q 'CALL \[ipv4\] \[eth0\] \[dhcp\]' && pass "set address dhcp" || fail "set address dhcp: $calls"
run netsh interface ip set dns name=eth0 source=static address=1.1.1.1
printf '%s' "$calls" | grep -q 'CALL \[dns\] \[eth0\] \[1.1.1.1\]' && pass "set dns static (named parameters)" || fail "set dns: $calls"
run netsh interface ip set dns eth0 dhcp
printf '%s' "$calls" | grep -q 'CALL \[dns\] \[eth0\] \[auto\]' && pass "set dns dhcp" || fail "set dns dhcp: $calls"
run netsh interface ip add dns name="eth0" 8.8.8.8 index=1
printf '%s' "$calls" | grep -q 'CALL \[dns\] \[eth0\] \[8.8.8.8,10.0.2.3\]' && pass "add dns index=1 puts it first" || fail "add dns: $calls"
run netsh interface ip show config
case "$out" in *'Configuration for interface "eth0"'*'DHCP enabled:                         Yes'*'IP Address:                           10.0.2.15'*'Default Gateway:                      10.0.2.2'*)
    pass "show config reads like Windows'" ;; *) fail "show config: $out" ;; esac

# netsh wlan
run netsh wlan show networks
case "$out" in *'SSID 1 : Cafe'*'WPA2-Personal'*) pass "wlan show networks" ;; *) fail "wlan show networks: $out" ;; esac
run netsh wlan connect name='My Cafe "5G"'
printf '%s' "$calls" | grep -qF 'CALL [wifi] [connect] [--ssid] [My Cafe "5G"]' && case "$out" in *"completed successfully"*) true ;; *) false ;; esac \
    && pass "wlan connect, an awkward name reaching sg-netctl whole" || fail "wlan connect: $calls / $out"

# refusals, and what is not ours
FAKE_DENY=1 run netsh interface ip set address eth0 dhcp
case "$out" in *"requires elevation"*) [ $code = 1 ] && pass "a refusal reads as Windows' elevation message, exit 1" || fail "denied exit $code" ;; *) fail "denied: $out" ;; esac
run netsh advfirewall set allprofiles state off
[ $code = 0 ] && [ -z "$calls" ] && pass "commands netsh does not know still succeed quietly" || fail "advfirewall ($code): $calls"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
