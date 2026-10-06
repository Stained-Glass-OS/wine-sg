#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.System.Profile.SystemIdentification (patches/sg/0961) gives a
# machine identifier that stays the same across restarts and differs from
# machine to machine, and CryptographicBuffer turns it into hex (0962).
# AnyDesk's service asks for it ("Could not get system ID (0x80040154)",
# then 0x80004001 for the hex) to know the machine again; David's installed
# AnyDesk came up with a new address after restarts.
#
#   WINE=/opt/wine-sg/bin/wine test/sysid-gate.sh
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

T=$(mktemp -d /var/tmp/sg-sysid.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/sysid-probe.c" -lruntimeobject -lole32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
run() { timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/$1"; "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w; }
run out1
run out2
"$WINE" reg add 'HKLM\SOFTWARE\Microsoft\Cryptography' /v MachineGuid /d 0b5c2a8e-6f1d-4c3a-9e7b-2d4f6a8c1e30 /f /reg:64 >/dev/null 2>&1
"$WINESERVER" -w
run out3
sed 's/^/      /' "$T/out1"
v() { sed -n "s/^$2 //p" "$T/$1"; }

[ "$(v out1 factory-hr)" = 00000000 ] && pass "Windows.System.Profile.SystemIdentification can be activated" || fail "activation: $(v out1 factory-hr)"
[ "$(v out1 publisher-hr)" = 00000000 ] && [ "$(v out1 publisher-len)" = 32 ] && pass "GetSystemIdForPublisher gives a 32-byte identifier" || fail "publisher id: $(v out1 publisher-hr) len $(v out1 publisher-len)"
[ "$(v out1 publisher-source)" = 3 ] && pass "its source is the registry (SystemIdentificationSource.Registry)" || fail "source: $(v out1 publisher-source)"
id1=$(v out1 publisher-id); id2=$(v out2 publisher-id); id3=$(v out3 publisher-id)
[ -n "$id1" ] && [ "$id1" = "$(v out1 publisher2-id)" ] && [ "$id1" = "$id2" ] && pass "the same identifier every time, after a restart too" || fail "not stable: $id1 / $(v out1 publisher2-id) / $id2"
[ -n "$id3" ] && [ "$id3" != "$id1" ] && pass "another machine (another MachineGuid) gets another identifier" || fail "same on another machine: $id3"
uid=$(v out1 user-id)
[ -n "$uid" ] && [ "$uid" != "$id1" ] && [ "$uid" = "$(v out2 user-id)" ] && pass "GetSystemIdForUser: an identifier of its own, also stable" || fail "user id: $uid"
[ "$(v out1 crypto-hr)" = 00000000 ] && [ "$(v out1 publisher-hex)" = "$id1" ] && pass "CryptographicBuffer.EncodeToHexString gives the identifier in hex" || fail "hex: $(v out1 publisher-hex-hr)$(v out1 publisher-hex)"
[ "$(v out1 publisher-copyhex)" = "$id1" ] && pass "CopyToByteArray / CreateFromByteArray keep the bytes" || fail "copy: $(v out1 publisher-copy-hr)$(v out1 publisher-copyhex)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
