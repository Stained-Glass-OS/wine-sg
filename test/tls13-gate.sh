#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# TLS 1.3 for schannel's clients (patches/sg/8415). Two local servers (openssl s_server on
# 127.0.0.1, a certificate made for the run; nothing leaves the machine): one TLS 1.3 only, one
# TLS 1.2 only. wininet and winhttp reach both (a server taking one version only is the proof of
# the version; where wininet reports it, it must be SP_PROT_TLS1_3_CLIENT / _TLS1_2_CLIENT); Protocols\TLS 1.3\Client DisabledByDefault=1, then Enabled=0, turn TLS 1.3
# off again (the 1.3-only server refused, the 1.2 one still reached). claws-mail.org is TLS 1.3
# only: SG Store's download failed with 12157.
#   WINE=/opt/wine-sg/bin/wine test/tls13-gate.sh
# Mutant: SG_MUTANT_NO_TLS13_CLIENT (dlls/secur32/schannel.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in x86_64-w64-mingw32-gcc openssl python3; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tls13.XXXXXX); S13=; S12=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$S13" ] && kill $S13 2>/dev/null; [ -n "$S12" ] && kill $S12 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/tls13-probe.c" "$HERE/tls13-http.c" -lwininet -lwinhttp ||
    { echo "FAIL  probe did not build"; exit 1; }
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj /CN=localhost -keyout "$T/key.pem" -out "$T/cert.pem" >/dev/null 2>&1 ||
    { echo "FAIL  no certificate"; exit 1; }
port() { python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])'; }
P13=$(port); P12=$(port)
openssl s_server -accept 127.0.0.1:$P13 -cert "$T/cert.pem" -key "$T/key.pem" -tls1_3 -www -quiet >/dev/null 2>&1 </dev/null & S13=$!
openssl s_server -accept 127.0.0.1:$P12 -cert "$T/cert.pem" -key "$T/key.pem" -tls1_2 -www -quiet >/dev/null 2>&1 </dev/null & S12=$!
sleep 1
echo | openssl s_client -connect 127.0.0.1:$P13 -tls1_2 >/dev/null 2>&1 && { echo "FAIL  the 1.3 server takes TLS 1.2"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
run() { (cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" 'C:\probe.exe' "$1" "$2" 2>/dev/null </dev/null | tr -d '\r' | tail -1); }
o=$(run inet $P13); case "$o" in "ok proto=0x2000"|"ok proto=?") pass "wininet reaches a TLS 1.3-only server with TLS 1.3 ($o)";; *) fail "wininet to the TLS 1.3-only server: $o";; esac
o=$(run http $P13); case "$o" in ok*) pass "winhttp reaches a TLS 1.3-only server";; *) fail "winhttp to the TLS 1.3-only server: $o";; esac
o=$(run inet $P12); case "$o" in "ok proto=0x800"|"ok proto=?") pass "wininet still reaches a TLS 1.2 server with TLS 1.2 ($o)";; *) fail "wininet to the TLS 1.2 server: $o";; esac
o=$(run http $P12); case "$o" in ok*) pass "winhttp still reaches a TLS 1.2 server";; *) fail "winhttp to the TLS 1.2 server: $o";; esac
K='HKLM\System\CurrentControlSet\Control\SecurityProviders\SCHANNEL\Protocols\TLS 1.3\Client'
"$WINE" reg add "$K" /v DisabledByDefault /t REG_DWORD /d 1 /f >/dev/null 2>&1; "$WINESERVER" -w
o=$(run inet $P13); case "$o" in fail*) pass "DisabledByDefault=1: the TLS 1.3-only server is refused";; *) fail "DisabledByDefault=1 ignored: $o";; esac
o=$(run inet $P12); case "$o" in "ok proto=0x800"|"ok proto=?") pass "DisabledByDefault=1: the TLS 1.2 server still reached";; *) fail "DisabledByDefault=1, TLS 1.2 server: $o";; esac
"$WINE" reg delete "$K" /v DisabledByDefault /f >/dev/null 2>&1
"$WINE" reg add "$K" /v Enabled /t REG_DWORD /d 0 /f >/dev/null 2>&1; "$WINESERVER" -w
o=$(run inet $P13); case "$o" in fail*) pass "Enabled=0: the TLS 1.3-only server is refused";; *) fail "Enabled=0 ignored: $o";; esac
"$WINE" reg add "$K" /v Enabled /t REG_DWORD /d 1 /f >/dev/null 2>&1; "$WINESERVER" -w
o=$(run inet $P13); case "$o" in "ok proto=0x2000"|"ok proto=?") pass "Enabled=1: TLS 1.3 again";; *) fail "Enabled=1: $o";; esac
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
