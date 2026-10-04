#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# schannel offers the ciphers Windows' does (patches/sg/0797): not
# ChaCha20-Poly1305, which has no ALG_ID -- a connection that chose it
# reported cipher 0 (Opera's installer: "unknown algorithm 23"), and
# programs that check a connection's strength refuse that. A server that
# prefers ChaCha20 but also has AES-GCM (TLS 1.2 and 1.3): the connection is
# AES, its cipher reported (CALG_AES_128/256). (A TLS 1.3-only server: the
# handshake fails with or without this -- schannel's client here has no TLS
# 1.3, as Windows 10's has none by default.)
#
#   WINE=/opt/wine-sg/bin/wine test/tlscipher-gate.sh   (mutant SG_MUTANT_CHACHA20)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0; SP=
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
for t in openssl python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-tlscipher.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$SP" ] && kill $SP 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/tlscipher-probe.c" -lsecur32 -lws2_32 || { fail "probe did not build"; exit 1; }
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/key.pem" -out "$T/cert.pem" -days 1 -subj /CN=localhost >/dev/null 2>&1
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
port() { python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])'; }
for v in 1.2 any; do
    P=$(port)
    if [ "$v" = 1.2 ]; then
        openssl s_server -quiet -tls1_2 -serverpref -cipher 'ECDHE-RSA-CHACHA20-POLY1305:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384' \
            -accept "127.0.0.1:$P" -cert "$T/cert.pem" -key "$T/key.pem" -naccept 1 </dev/null >/dev/null 2>&1 & SP=$!
    else
        openssl s_server -quiet -serverpref -ciphersuites 'TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384' -cipher 'ECDHE-RSA-CHACHA20-POLY1305:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384' \
            -accept "127.0.0.1:$P" -cert "$T/cert.pem" -key "$T/key.pem" -naccept 1 </dev/null >/dev/null 2>&1 & SP=$!
    fi
    sleep 1
    out=$(timeout 60 "$WINE" "$T/probe.exe" "$P" 2>/dev/null | tr -d '\r')
    kill $SP 2>/dev/null; SP=
    case "$out" in
        "CIPHER 660e "*|"CIPHER 6610 "*) pass "TLS $v (version), a server that prefers ChaCha20: AES, reported ($out)";;
        "CIPHER 0000 "*) fail "TLS $v: a cipher with no ALG_ID ($out)";;
        *) fail "TLS $v: '$out'";;
    esac
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
