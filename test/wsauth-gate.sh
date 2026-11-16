#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# webservices HTTP header authentication (7811, N-WEBSERVICES): WsCreateChannel ignored its security
# description, so an HTTP channel never answered a server's 401 with the caller's credential, and any HTTP
# error status was read as if it were the SOAP reply. Now:
#  - WS_HTTP_HEADER_AUTH_SECURITY_BINDING with a WS_STRING_WINDOWS_INTEGRATED_AUTH_CREDENTIAL: challenged,
#    the channel answers with the scheme the caller allows (Basic here) and the reply comes back;
#  - a wrong password, a server that refuses everyone, or no security description: WS_E_ENDPOINT_ACCESS_DENIED;
#  - the credential goes only with a scheme the caller allows: a channel allowing NTLM alone does not send it
#    to a Basic challenge, and the user's own (default) credentials never go to Basic or Digest;
#  - HTTP 404: WS_E_ENDPOINT_NOT_FOUND;
#  - an unknown credential type: E_NOTIMPL; scheme bits that do not exist: E_INVALIDARG.
# The account is a made-up test pair checked by a local Python server (wsauth-server.py). Runs in a network
# namespace of its own (unshare -rn, loopback only).
#   WINE=/opt/wine-sg/bin/wine test/wsauth-gate.sh   (mutant SG_MUTANT_WS_AUTH_ANY_SCHEME, webservices channel.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" unshare ip python3; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -rn true 2>/dev/null || { echo "SKIP: no unprivileged user and network namespaces"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wsauth.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/wsauth-probe.exe" "$HERE/wsauth-probe.c" \
    -idirafter "${WINE_INCLUDE:-/opt/wine-sg/include/wine/windows}" -lwebservices \
    || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
cat > "$T/inner.sh" <<INNER
set -u
cd "$T"
ip link set lo up
python3 "$HERE/wsauth-server.py" 18391 "$T/server.log" &
SERVER=\$!
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/wsauth-probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" wsauth-probe.exe > "$T/out" 2>"\${WSA_LOG:-/dev/null}" </dev/null)
"$WINESERVER" -k 2>/dev/null
kill \$SERVER
INNER
unshare -rn sh "$T/inner.sh"
out=$(tr -d '\r' < "$T/out" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect NoCred "0x803d0005 0" "no security description, challenged: WS_E_ENDPOINT_ACCESS_DENIED"
expect Basic "0 4321" "HTTP header auth (Basic allowed): the challenge answered, the reply read"
expect WrongPass "0x803d0005 0" "a wrong password: WS_E_ENDPOINT_ACCESS_DENIED"
expect NtlmOnly "0x803d0005 0" "NTLM allowed, the server offers Basic: refused, the credential not sent"
expect DefaultsBasic "0x803d0005 0" "the user's own credentials are not sent to a Basic challenge"
expect Deny "0x803d0005 0" "a server that refuses everyone: WS_E_ENDPOINT_ACCESS_DENIED"
expect Missing "0x803d000d 0" "HTTP 404: WS_E_ENDPOINT_NOT_FOUND"
expect BadCred "create 0x80004001" "an unknown credential type: E_NOTIMPL"
expect BadScheme "create 0x80070057" "scheme bits that do not exist: E_INVALIDARG"
expect Done 1 "the probe ran to the end"
good=$(grep -c '^/svc Basic c2d0ZXN0OnNncGFzcw==$' "$T/server.log" 2>/dev/null)
[ "$good" = 1 ] && pass "the right credential reached the server once, for the Basic channel only" \
    || fail "the right credential reached the server $good times (wanted 1)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
