#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# VPN connections through RAS (patches/sg/0400): rasapi32 and rasdial on
# sg-netctl's vpn commands -- here a stand-in that records what it was asked
# and keeps a WireGuard and an OpenVPN connection. Programs enumerate the
# entries (RasEnumEntries) and the connected ones (RasEnumConnections, A and
# W, with the caller's structure size), dial by name (RasDial; a password
# goes to sg-netctl on standard input, never its command line), read the
# status and hang up; rasdial prints Windows' answers and exit codes.
#
#   WINE=/opt/wine-sg/bin/wine test/rasdial-gate.sh
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
T=$(mktemp -d /var/tmp/sg-rasdial.XXXXXX)
# a scratch home: a prefix links its user folders to $HOME's, and a test must
# never reach the real one
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" XDG_DESKTOP_DIR="$T/home/Desktop"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/rasdial-probe.exe" "$HERE/rasdial-probe.c" -lrasapi32 || { fail "probe did not build"; exit 1; }
cat > "$T/fake-netctl" <<'FAKE'
#!/bin/sh
{ printf 'CALL'; for a in "$@"; do printf ' [%s]' "$a"; done; printf '\n'; } >> "$FAKE_LOG"
[ "$1" = vpn ] || { echo "ERROR invalid unknown command"; exit 2; }
state() { [ -f "$FAKE_STATE.$1" ] && echo connected || echo disconnected; }
case "$2" in
list)
    printf 'VPN 1111-aaaa\twireguard\t%s\tOffice VPN\n' "$(state 1111-aaaa)"
    printf 'VPN 2222-bbbb\topenvpn\t%s\tBranch\n' "$(state 2222-bbbb)"
    echo OK ;;
connect)
    case "$3" in 1111-aaaa|2222-bbbb) ;; *) echo "ERROR notfound no VPN connection called $3"; exit 1 ;; esac
    if [ "${4:-}" = --password-stdin ]; then
        IFS= read -r pw; printf 'STDIN [%s]\n' "$pw" >> "$FAKE_LOG"
        [ "$pw" = right ] || { echo "ERROR auth the VPN server did not accept the sign-in"; exit 1; }
    fi
    touch "$FAKE_STATE.$3"; echo "CONNECTED x"; echo OK ;;
disconnect) rm -f "$FAKE_STATE.$3"; echo "DISCONNECTED x"; echo OK ;;
*) echo "ERROR invalid"; exit 2 ;;
esac
FAKE
chmod +x "$T/fake-netctl"
export SG_NETCTL="$T/fake-netctl" FAKE_LOG="$T/calls" FAKE_STATE="$T/state"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/rasdial-probe.exe" "$WINEPREFIX/drive_c/"

run() { : > "$T/calls"; timeout -s KILL 90 "$WINE" "$@" > "$T/out" 2>/dev/null; code=$?; out=$(tr -d '\r' < "$T/out"); calls=$(cat "$T/calls"); }

run C:\\rasdial-probe.exe
case "$out" in *"entries-size-query=603 size=2096 count=2"*) pass "RasEnumEntries says how much room two entries need" ;; *) fail "size query: $out" ;; esac
case "$out" in *"entries=0 count=2 [Office VPN] [Branch]"*) pass "and lists both VPN connections as phone-book entries" ;; *) fail "entries: $out" ;; esac
case "$out" in *"badsize=632"*) pass "a wrong RASCONN size is ERROR_INVALID_SIZE" ;; *) fail "bad size: $out" ;; esac
case "$out" in *"connections=0 count=0"*) pass "nothing is connected yet" ;; *) fail "connections: $out" ;; esac

run rasdial
case "$out" in *"No connections"*"Command completed successfully."*) [ $code = 0 ] && pass "rasdial with no arguments: No connections" || fail "rasdial exit $code" ;; *) fail "rasdial: $out" ;; esac

run rasdial "Office VPN"
case "$out" in *"Connecting to Office VPN..."*"Successfully connected to Office VPN."*) [ $code = 0 ] && pass "rasdial NAME dials it" || fail "rasdial NAME exit $code" ;; *) fail "rasdial NAME: $out" ;; esac
printf '%s' "$calls" | grep -q 'CALL \[vpn\] \[connect\] \[1111-aaaa\]$' && pass "by its connection's id, with no password" || fail "connect call: $calls"

run C:\\rasdial-probe.exe
case "$out" in *"connections=0 count=1 [Office VPN|vpn|WireGuard] status=0 state=connected"*) pass "RasEnumConnections lists it as a VPN device, RasGetConnectStatus says connected" ;; *) fail "connected: $out" ;; esac
case "$out" in *"connectionsA=0 count=1 Office VPN"*) pass "and so does RasEnumConnectionsA" ;; *) fail "A: $out" ;; esac
case "$out" in *"status-bogus=6"*) pass "a handle that is no connection is ERROR_INVALID_HANDLE" ;; *) fail "bogus handle: $out" ;; esac

run rasdial
case "$out" in *"Connected to"*"Office VPN"*) pass "rasdial lists the connection" ;; *) fail "rasdial list: $out" ;; esac

run cmd /v:on /c "rasdial Branch alice wrong & echo EXIT=!errorlevel!"
case "$out" in *"Remote Access error 691"*) case "$out" in *EXIT=691*) true ;; *) false ;; esac && pass "a refused password is error 691, and the exit code" || fail "exit: $out" ;; *) fail "wrong password: $out" ;; esac
printf '%s' "$calls" | grep -q 'STDIN \[wrong\]' && ! printf '%s' "$calls" | grep -q 'CALL.*wrong' \
    && pass "the password reaches sg-netctl on standard input, never its command line" || fail "password path: $calls"

run C:\\rasdial-probe.exe dial Branch right
case "$out" in *"dial=0 handle=yes"*) pass "RasDialA with the right password connects" ;; *) fail "RasDialA: $out" ;; esac

run cmd /v:on /c "rasdial Nowhere & echo EXIT=!errorlevel!"
case "$out" in *"Remote Access error 623"*) case "$out" in *EXIT=623*) true ;; *) false ;; esac && pass "an unknown entry is error 623" || fail "exit: $out" ;; *) fail "unknown: $out" ;; esac

run rasdial "Office VPN" /disconnect
[ $code = 0 ] && printf '%s' "$calls" | grep -q 'CALL \[vpn\] \[disconnect\] \[1111-aaaa\]' && pass "rasdial NAME /DISCONNECT hangs it up" || fail "disconnect ($code): $out $calls"
run rasdial /disconnect
printf '%s' "$calls" | grep -q 'CALL \[vpn\] \[disconnect\] \[2222-bbbb\]' && pass "rasdial /DISCONNECT hangs up the rest" || fail "disconnect all: $calls"
run rasdial
case "$out" in *"No connections"*) pass "and nothing is left connected" ;; *) fail "after: $out" ;; esac

exit $RC
