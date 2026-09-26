#!/bin/sh
# What Discord and Spotify need at start (patches/sg/0380-0382), found by the
# compat suite's entries for them:
#
#  - the SSL chain policy honours CERT_CHAIN_POLICY_IGNORE_*_REV_UNKNOWN_FLAG:
#    a certificate whose CRL cannot be fetched passes when the caller says to
#    ignore unknown revocation, as Rust's schannel crate does (0380; Discord's
#    updater: "Update failed -- retrying" forever, CERT_E_REVOCATION_FAILURE);
#  - NtQueryInformationProcess answers ProcessHandleTable and
#    ProcessHandleCount, in 64-bit and 32-bit processes (0381; Chromium's
#    sandbox closes handles by listing them: Spotify's 32-bit renderers exited
#    with SBOX_FATAL_CLOSEHANDLES and the browser process then crashed);
#  - the shell makes its tray settings key (0382; Squirrel installers:
#    Discord's installer never returned);
#  - given part of a TLS record, DecryptMessage says how many bytes are
#    missing in pBuffers[1] too (0383; Rust's schannel crate reads it there:
#    Discord's updater waited minutes at the end of every download). Against
#    a local openssl s_server with a throwaway certificate; skipped without
#    openssl.
#
#   WINE=/opt/wine-sg/bin/wine test/commapps-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-commapps.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DISPLAY=
# a scratch HOME: the prefix's Desktop, Documents... link to $HOME's
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" \
    XDG_DESKTOP_DIR="$T/home/Desktop" WINEDLLOVERRIDES="winemenubuilder.exe=d"
mkdir -p "$HOME/Desktop"
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/cp64.exe" "$HERE/commapps-probe.c" -lcrypt32 -lsecur32 -lws2_32 &&
TMPDIR=/var/tmp "$MINGW32" -O2 -o "$T/cp32.exe" "$HERE/commapps-probe.c" -lcrypt32 -lsecur32 -lws2_32 ||
    { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/cp64.exe" "$T/cp32.exe" "$WINEPREFIX/drive_c/"

out=$(timeout -s KILL 120 "$WINE" 'C:\cp64.exe' 2>/dev/null | tr -d '\r')
out32=$(timeout -s KILL 120 "$WINE" 'C:\cp32.exe' handles 2>/dev/null | tr -d '\r')
outtls=""
if command -v openssl >/dev/null && command -v python3 >/dev/null; then
    openssl req -x509 -newkey rsa:2048 -nodes -keyout "$T/key.pem" -out "$T/cert.pem" -days 2 \
        -subj /CN=probe.invalid >/dev/null 2>&1
    port=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
    openssl s_server -quiet -www -accept "127.0.0.1:$port" -cert "$T/cert.pem" -key "$T/key.pem" \
        -tls1_2 >/dev/null 2>&1 &
    srv=$!
    for i in 1 2 3 4 5 6 7 8 9 10; do python3 -c "import socket; socket.create_connection(('127.0.0.1',$port))" 2>/dev/null && break; sleep 0.5; done
    outtls=$(timeout -s KILL 60 "$WINE" 'C:\cp64.exe' tls "$port" 2>/dev/null | tr -d '\r')
    kill "$srv" 2>/dev/null
fi
printf '%s\n' "$out" | sed '/^$/d; s/^/      /'
printf '%s\n' "$outtls" | sed '/^$/d; s/^/      /'
printf '%s\n' "$out32" | sed '/^$/d; s/^/  32  /'
check() { if printf '%s\n' "$1" | grep -q "^$2"; then pass "$3"; else fail "$3 ($(printf '%s\n' "$1" | grep "^${2%%=*}=" | head -1 || echo none))"; fi; }

check "$out" 'revocation_offline=1'          "a certificate whose CRL cannot be fetched has unknown (offline) revocation"
check "$out" 'revocation_unknown_ignored=1'  "the SSL policy passes it with CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS"
check "$out" 'revocation_unknown_strict=1'   "... and still fails it (CERT_E_REVOCATION_FAILURE) without them"
check "$out" 'handle_table=1'                "ProcessHandleTable lists the process's handles (64-bit)"
check "$out" 'handle_count=1'                "ProcessHandleCount counts them (64-bit)"
check "$out32" 'handle_table=1'              "ProcessHandleTable lists the process's handles (32-bit, WoW64)"
check "$out32" 'handle_count=1'              "ProcessHandleCount counts them (32-bit, WoW64)"
if [ -n "$outtls" ]; then
check "$outtls" 'tls_handshake=1'            "a TLS 1.2 handshake with a local server through schannel"
check "$outtls" 'tls_incomplete=1'           "DecryptMessage on part of a record: SEC_E_INCOMPLETE_MESSAGE"
check "$outtls" 'tls_missing0=1'             "... pBuffers[0] is SECBUFFER_MISSING with the bytes still to come"
check "$outtls" 'tls_missing1=1'             "... and so is pBuffers[1], with the same count (not the record's size)"
check "$outtls" 'tls_decrypt=1'              "the whole record then decrypts (\"HTTP...\")"
else echo "SKIP  schannel DecryptMessage (no openssl/python3)"; fi
check "$out" 'traynotify_key=1'              "the shell's tray settings key exists once the desktop is up"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
