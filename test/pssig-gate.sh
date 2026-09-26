#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# PowerShell script signatures (patches/sg/0404) and the certificate stores
# PowerShell's cert: drive lists (0405).
#
# 0404: a .ps1/.psm1/.psd1 is its own subject (the PowerShell SIP): its
#   signature is read from the "# SIG #" block and its digest checked, so
#   WinVerifyTrust -- and Get-AuthenticodeSignature, execution policy
#   AllSigned, Install-Module's publisher check -- says Valid, HashMismatch or
#   NotSigned instead of "unknown subject". pssig-make.py signs scripts with
#   a throwaway root the way PowerShell does.
# 0405: CertEnumSystemStore lists the machine's Root store once (twice broke
#   all of cert:\LocalMachine), and both locations have Windows' standard
#   stores (AuthRoot, TrustedPublisher... -- Install-Module walks them).
#
#   WINE=/opt/wine-sg/bin/wine test/pssig-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v openssl >/dev/null && python3 -c 'import cryptography' 2>/dev/null || { echo "SKIP: needs openssl and python3-cryptography"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-pssig.XXXXXX)
mkdir -p "$T/home" "$T/sig"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" XDG_DESKTOP_DIR="$T/home/Desktop"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/pssig-probe.exe" "$HERE/pssig-probe.c" -lwintrust -lcrypt32 || { fail "probe did not build"; exit 1; }
python3 "$HERE/pssig-make.py" "$T/sig" || { fail "could not sign the test scripts"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/pssig-probe.exe" "$T/sig/root.der" "$T/sig/signed.ps1" "$T/sig/bom.psm1" "$C/"
sed 's/signed by SGTEST/signed by SGTESX/' "$T/sig/signed.ps1" > "$C/tampered.ps1"
printf 'Write-Output "not signed"\r\n' > "$C/plain.ps1"
p() { timeout -s KILL 60 "$WINE" 'C:\pssig-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }

out=$(p addroot 'C:\root.der')
case "$out" in *addroot=1*) ;; *) fail "could not trust the test root: $out" ;; esac
out=$(p verify 'C:\signed.ps1'); echo "signed: $out"
case "$out" in *'sip={603bcc1f-4b59-4e08}'*) pass "a .ps1 is the PowerShell SIP's subject" ;; *) fail "subject: $out" ;; esac
case "$out" in *trust=00000000*) pass "a signed script verifies (WinVerifyTrust)" ;; *) fail "signed script: $out" ;; esac
out=$(p verify 'C:\bom.psm1'); echo "bom: $out"
case "$out" in *trust=00000000*) pass "and so does a module saved with a byte-order mark" ;; *) fail "UTF-8 BOM module: $out" ;; esac
out=$(p verify 'C:\tampered.ps1'); echo "tampered: $out"
case "$out" in *trust=80096010*) pass "a script changed after signing is TRUST_E_BAD_DIGEST" ;; *) fail "tampered: $out" ;; esac
out=$(p verify 'C:\plain.ps1'); echo "plain: $out"
case "$out" in *trust=800b0100*) pass "an unsigned script is TRUST_E_NOSIGNATURE" ;; *) fail "unsigned: $out" ;; esac

out=$(p stores); echo "$out"
m=$(printf '%s\n' "$out" | sed -n 's/^machine://p'); u=$(printf '%s\n' "$out" | sed -n 's/^user://p')
[ "$(printf '%s\n' $m | grep -cx Root)" = 1 ] && pass "the machine's Root store is listed once" || fail "Root listed: $m"
ok=1; for s in AuthRoot CA Disallowed My Root Trust TrustedPeople TrustedPublisher; do
    printf '%s\n' $m | grep -qx "$s" && printf '%s\n' $u | grep -qx "$s" || ok=0; done
[ $ok = 1 ] && pass "both locations have the standard stores (AuthRoot, TrustedPublisher, ...)" || fail "stores: $out"
exit $RC
