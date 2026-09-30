#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Service trigger info (patches/sg/0520): ChangeServiceConfig2 and
# QueryServiceConfig2 at SERVICE_CONFIG_TRIGGER_INFO.  Microsoft OneDrive's
# per-machine installer registers its "OneDrive Updater Service" with a
# trigger; Wine's service RPC had no arm for level 8 (RPC_S_INVALID_TAG), so
# the install failed with 0x800706c5 and was rolled back.  The probe sets two
# triggers (a subtype GUID each, binary and string data), reads them back in
# the W and A forms, finds them where Windows keeps them
# (Services\<name>\TriggerInfo\<n>), checks the validation, sets them in the A
# form, and clears them.
#
#   WINE=/opt/wine-sg/bin/wine test/svctrigger-gate.sh
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
T=$(mktemp -d /var/tmp/sg-svctrigger.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/svctrigger-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r' > "$T/out"
sed 's/^/      /' "$T/out"
v() { sed -n "s/^$1 //p" "$T/out"; }
chk() { [ "$(v "$1")" = "$2" ] && pass "$3" || fail "$3: $1 '$(v "$1")' (expected '$2')"; }

chk empty "122 1 0 1" "no triggers: just the structure (cTriggers 0, no array), size asked for first"
chk set 0 "ChangeServiceConfig2W(SERVICE_CONFIG_TRIGGER_INFO) succeeds (was RPC_S_INVALID_TAG)"
chk count "122 2" "two triggers read back"
chk t0 "2 1 1 0" "IP address availability, start, its subtype GUID"
chk t1 "20 2 1 2" "custom, stop, its GUID and two data items"
chk d0 "1 3 1" "binary data item"
chk d1 "2 10 1" "string data item (multi-sz, UTF-16)"
chk inside 1 "everything laid out inside the caller's buffer"
chk ansi "5 1" "QueryServiceConfig2A: the string item in the A code page"
chk registry "2 1 1" "stored as Windows does: Services\\<name>\\TriggerInfo\\0 Type, Action, GUID"
chk badaction 87 "an unknown action is ERROR_INVALID_PARAMETER"
chk badtype 87 "an unknown trigger type is ERROR_INVALID_PARAMETER"
chk seta "1 8 1" "ChangeServiceConfig2A: string data converted to UTF-16"
chk cleared "0 1" "no triggers removes them all (TriggerInfo gone)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
