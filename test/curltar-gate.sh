#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg's curl.exe and tar.exe (issue #16, tools/build-addons.sh) and 9410: Windows (10
# 1803 on) ships curl.exe (curl with Schannel, Windows' own TLS) and tar.exe
# (bsdtar) in System32 and SysWOW64; Wine has neither, so scripts and tools that
# call them failed ("curl is not included"). tools/build-addons.sh builds them
# from upstream curl and libarchive with the mingw cross compilers; wineboot
# puts them in place like Wine's own programs.
#
# Checks: both are in system32 and syswow64 (64- and 32-bit), curl names
# Schannel, fetches over HTTP and over HTTPS against a local server whose
# certificate comes from the gate's own authority: refused while that is not
# a trusted root (exit 60) -- Wine's Schannel did not check certificates at
# all, 9410 checks them as Windows' does: chain, name, revocation when asked --
# fetched once it is, refused for another name; -k fetches; --compressed
# decodes gzip; tar makes a .tar.gz Linux tar reads, and extracts a .zip, as
# Windows' tar does.
# Needs python3 and openssl; opens sockets on 127.0.0.1 only: run it in a
# private network namespace (/var/tmp/sgwork/netns-run.sh) on a shared host.
#   WINE=... test/curltar-gate.sh
# Mutants: SG_MUTANT_SCHANNEL_NO_VERIFY (dlls/secur32/schannel.c); SG_MUTANT_NO_ADDONS=1 build.sh.
set -u
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
command -v python3 >/dev/null && command -v openssl >/dev/null || { echo "SKIP: needs python3 and openssl"; exit 77; }
W=$(mktemp -d /var/tmp/sg-curltar.XXXXXX)
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
SRV=""
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; sleep 1; rm -rf "$W"; }
trap cleanup EXIT
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
unset DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
S32=$WINEPREFIX/drive_c/windows/system32 S64=$WINEPREFIX/drive_c/windows/syswow64
for p in curl tar; do
    if [ -f "$S32/$p.exe" ] && [ -f "$S64/$p.exe" ]; then pass "$p.exe is in system32 and syswow64"
    else fail "$p.exe missing (system32: $(ls "$S32/$p.exe" 2>&1 | head -c 80))"; fi
done
v=$("$WINE" curl.exe --version 2>/dev/null | tr -d '\r')
grep -q 'Schannel' <<<"$v" && pass "curl uses Schannel ($(head -1 <<<"$v"))" || fail "curl --version: $(head -1 <<<"$v")"
v32=$("$WINE" 'C:\windows\syswow64\curl.exe' --version 2>/dev/null | tr -d '\r' | head -1)
grep -q 'i686' <<<"$v32" && pass "the 32-bit one runs ($v32)" || fail "32-bit curl: '$v32'"
v=$("$WINE" tar.exe --version 2>/dev/null | tr -d '\r' | head -1)
case "$v" in bsdtar*libarchive*zlib*) pass "tar is bsdtar ($v)" ;; *) fail "tar --version: '$v'" ;; esac

# a local web server, HTTP and HTTPS (a certificate of its own)
mkdir -p "$W/www"
head -c 300000 /dev/urandom > "$W/www/blob.bin"
python3 -c 'import sys; sys.stdout.write("compressible line\n" * 5000)' > "$W/www/text.txt"
# a certificate authority of the gate's own and the server's certificate from it (127.0.0.1, localhost)
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$W/ca.key" -out "$W/ca.pem" -days 2 -subj "/CN=SG Gate Test CA" \
    -addext basicConstraints=critical,CA:TRUE -addext keyUsage=critical,keyCertSign,cRLSign >/dev/null 2>&1
openssl req -newkey rsa:2048 -nodes -keyout "$W/key.pem" -out "$W/srv.csr" -subj /CN=localhost >/dev/null 2>&1
printf 'subjectAltName=IP:127.0.0.1,DNS:localhost\nextendedKeyUsage=serverAuth\n' > "$W/ext.cnf"
openssl x509 -req -in "$W/srv.csr" -CA "$W/ca.pem" -CAkey "$W/ca.key" -CAcreateserial -out "$W/cert.pem" -days 2 \
    -extfile "$W/ext.cnf" >/dev/null 2>&1
openssl x509 -in "$W/ca.pem" -outform der -out "$WINEPREFIX/drive_c/ca.cer"
cat > "$W/srv.py" <<'EOF'
import gzip, http.server, os, socketserver, ssl, sys, threading
root = sys.argv[1]
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=root, **k)
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == '/gz' and 'gzip' in self.headers.get('Accept-Encoding', ''):
            body = gzip.compress(open(os.path.join(root, 'text.txt'), 'rb').read())
            self.send_response(200); self.send_header('Content-Encoding', 'gzip')
            self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body); return
        return super().do_GET()
class S(socketserver.ThreadingMixIn, http.server.HTTPServer): daemon_threads = True
plain = S(('127.0.0.1', 0), H)
tls = S(('127.0.0.1', 0), H)
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain(sys.argv[2], sys.argv[3])
tls.socket = ctx.wrap_socket(tls.socket, server_side=True)
print(plain.server_address[1], tls.server_address[1], flush=True)
threading.Thread(target=plain.serve_forever, daemon=True).start()
tls.serve_forever()
EOF
python3 "$W/srv.py" "$W/www" "$W/cert.pem" "$W/key.pem" > "$W/ports" 2>/dev/null &
SRV=$!
for _ in $(seq 50); do [ -s "$W/ports" ] && break; sleep 0.1; done
read -r HP SP < "$W/ports"
out='C:\out.bin'
o=$WINEPREFIX/drive_c/out.bin
rm -f "$o"; "$WINE" curl.exe -s -o "$out" "http://127.0.0.1:$HP/blob.bin" >/dev/null 2>&1
cmp -s "$o" "$W/www/blob.bin" && pass "curl fetches over HTTP" || fail "HTTP download"
# Schannel checks the server's certificate itself, as Windows' does (wine-sg 9410): Wine's took any
rm -f "$o"; "$WINE" curl.exe -s --ssl-no-revoke -o "$out" "https://127.0.0.1:$SP/blob.bin" >/dev/null 2>&1; rc=$?
[ "$rc" = 60 ] && [ ! -s "$o" ] && pass "HTTPS with a certificate from an untrusted authority is refused (exit 60)" \
    || fail "untrusted HTTPS: exit $rc"
"$WINE" certutil -addstore Root 'C:\ca.cer' >/dev/null 2>&1
rm -f "$o"; "$WINE" curl.exe -s --ssl-no-revoke -o "$out" "https://127.0.0.1:$SP/blob.bin" >/dev/null 2>&1; rc=$?
cmp -s "$o" "$W/www/blob.bin" && pass "once its authority is a trusted root, the same server is fetched from" \
    || fail "trusted HTTPS: exit $rc"
rm -f "$o"; "$WINE" curl.exe -s --ssl-no-revoke --resolve "wrong.example:$SP:127.0.0.1" -o "$out" \
    "https://wrong.example:$SP/blob.bin" >/dev/null 2>&1; rc=$?
[ "$rc" = 60 ] && [ ! -s "$o" ] && pass "a certificate for another name is refused (exit 60)" || fail "wrong name: exit $rc"
# by default curl asks Schannel to check revocation; this certificate names no CRL or OCSP responder,
# and Windows' Schannel then fails (CRYPT_E_NO_REVOCATION_CHECK, curl exit 35) -- best effort lets it by
rm -f "$o"; "$WINE" curl.exe -s -o "$out" "https://localhost:$SP/blob.bin" >/dev/null 2>&1; rc=$?
[ "$rc" = 35 ] && pass "without a revocation source the check fails, as on Windows (exit 35)" || fail "revocation default: exit $rc"
rm -f "$o"; "$WINE" curl.exe -s --ssl-revoke-best-effort -o "$out" "https://localhost:$SP/blob.bin" >/dev/null 2>&1; rc=$?
cmp -s "$o" "$W/www/blob.bin" && pass "--ssl-revoke-best-effort fetches (SCH_CRED_IGNORE_NO_REVOCATION_CHECK)" \
    || fail "best effort: exit $rc"
rm -f "$o"; "$WINE" curl.exe -s -k --ssl-no-revoke -o "$out" "https://127.0.0.1:$SP/blob.bin" >/dev/null 2>&1
cmp -s "$o" "$W/www/blob.bin" && pass "curl -k fetches over HTTPS (Schannel handshake)" || fail "HTTPS -k download"
rm -f "$o"; "$WINE" curl.exe -s --compressed -o "$out" "http://127.0.0.1:$HP/gz" >/dev/null 2>&1
cmp -s "$o" "$W/www/text.txt" && pass "curl --compressed decodes gzip" || fail "--compressed"

# tar: a .tar.gz of a folder, read back by Linux tar; a .zip extracted
mkdir -p "$WINEPREFIX/drive_c/src/sub"
printf 'one\n' > "$WINEPREFIX/drive_c/src/a.txt"; printf 'two\n' > "$WINEPREFIX/drive_c/src/sub/b.txt"
"$WINE" tar.exe -czf 'C:\pack.tgz' -C 'C:\src' . >/dev/null 2>&1
list=$(tar -tzf "$WINEPREFIX/drive_c/pack.tgz" 2>/dev/null | sed 's|^\./||' | sort | tr '\n' ' ')
case "$list" in *a.txt*sub/b.txt*) pass "tar -czf makes a gzip tarball ($list)" ;; *) fail "tar -czf: '$list'" ;; esac
python3 -c "import zipfile,sys; z=zipfile.ZipFile(sys.argv[1],'w',zipfile.ZIP_DEFLATED); z.writestr('dir/z.txt','zipped\n'); z.close()" \
    "$WINEPREFIX/drive_c/in.zip"
mkdir -p "$WINEPREFIX/drive_c/unz"
"$WINE" tar.exe -xf 'C:\in.zip' -C 'C:\unz' >/dev/null 2>&1
[ "$(cat "$WINEPREFIX/drive_c/unz/dir/z.txt" 2>/dev/null)" = zipped ] && pass "tar -xf extracts a .zip" || fail "tar -xf zip"
"$WINESERVER" -k 2>/dev/null

if [ "$fails" -eq 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL ($fails)"; exit 1
