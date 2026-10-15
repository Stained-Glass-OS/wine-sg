#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ncrypt: NCryptExportKey PKCS #8 blobs checked with the host openssl (patch 3721)
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-ncrypt-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastb-ncrypt.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastb-ncrypt-probe.c" -lncrypt -lbcrypt \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 env DISPLAY= "$WINE" probe.exe 2>/dev/null </dev/null | tr -d '\r') > "$T/probe.out"
cat "$T/probe.out" | grep -v '^P8 \|^PUB '
# openssl must accept every key, find it consistent, and see the public key the provider exports
command -v openssl >/dev/null || { echo "SKIP: openssl missing (key check)"; grep -qx 'RESULT: PASS' "$T/probe.out"; exit $?; }
bad=0
while read -r tag label hexdata; do
    [ "$tag" = P8 ] || continue
    printf '%s' "$hexdata" | python3 -c 'import sys,binascii; sys.stdout.buffer.write(binascii.unhexlify(sys.stdin.read()))' > "$T/$label.der"
    if openssl pkey -inform DER -in "$T/$label.der" -check -noout >"$T/$label.chk" 2>&1; then echo "PASS  openssl accepts $label ($(tr -d '\n' < "$T/$label.chk"))"
    else echo "FAIL  openssl rejects $label: $(cat "$T/$label.chk")"; bad=1; continue; fi
    python3 - "$label" "$T/$label.der" <<'PY2' || bad=1
import sys
label, path = sys.argv[1:3]
d = open(path, 'rb').read()
errs = []
def tlv(buf, i):
    tag = buf[i]; l = buf[i + 1]; i += 2
    if l & 0x80:
        n = l & 0x7f; l = int.from_bytes(buf[i:i + n], 'big'); i += n
    return tag, i, l
def walk(buf, depth=0):
    i = 0
    while i < len(buf):
        tag, st, l = tlv(buf, i)
        body = buf[st:st + l]
        if tag == 0x02:
            if body[0] & 0x80: errs.append('negative INTEGER')
            if len(body) > 1 and body[0] == 0 and not body[1] & 0x80: errs.append('non-minimal INTEGER')
        if tag in (0x30, 0x04, 0xa1) and depth < 3:
            try: walk(body, depth + 1)
            except Exception: pass
        if tag == 0xa1: seen.append(1)
        i = st + l
    return
seen = []
walk(d)
if label.startswith('rsa') is False and not seen: errs.append('EC private key without the public key [1]')
print(('FAIL  DER of %s: %s' % (label, errs)) if errs else 'PASS  DER of %s is well formed (positive minimal integers%s)' % (label, '' if label.startswith('rsa') else ', public key present'))
sys.exit(1 if errs else 0)
PY2
    openssl pkey -inform DER -in "$T/$label.der" -pubout -outform DER 2>/dev/null | python3 -c 'import sys,binascii; print(binascii.hexlify(sys.stdin.buffer.read()).decode().upper())' > "$T/$label.spki"
    pub=$(grep "^PUB $label " "$T/probe.out" | cut -d' ' -f3)
    python3 - "$label" "$pub" "$(cat "$T/$label.spki")" <<'PY' || bad=1
import sys
label, pub, spki = sys.argv[1:4]
if label.startswith('rsa'):
    cbexp = int.from_bytes(bytes.fromhex(pub[16:24]), 'little')
    want = pub[48 + 2 * cbexp:]
else:
    want = '04' + pub[16:]
ok = want and want in spki
print(('PASS  ' if ok else 'FAIL  ') + 'public key of ' + label + ' matches the provider\'s export')
sys.exit(0 if ok else 1)
PY
done < "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && [ $bad = 0 ] && exit 0
exit 1
