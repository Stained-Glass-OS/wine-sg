#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Per-program compatibility settings (patches/sg/0420): what the Compatibility
# tab of a program's Properties (sg-shell's sgcompat) stores, applied when the
# program starts -- however it starts.
#
#   AppDefaults\<exe>\Environment\NAME=value   set for it (empty: removed)
#   AppDefaults\<exe>\LaunchArgs               appended to its command line
#   AppDefaults\<exe>\Version                  Wine's own per-program version
#   AppCompatFlags\Layers\<path> RUNASADMIN    CreateProcess asks for elevation
#                                              (gated in elevreq-gate.sh)
#
#   WINE=/opt/wine-sg/bin/wine test/appcompat-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-appcompat.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/appcompat-probe.exe" "$HERE/appcompat-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/appcompat-probe.exe" "$C/appcompat-probe.exe"
cp "$T/appcompat-probe.exe" "$C/appcompat-admin.exe"
reg() { "$WINE" reg "$@" >/dev/null 2>&1; }
K='HKCU\Software\Wine\AppDefaults\appcompat-probe.exe'
reg add "$K\\Environment" /v SGTEST_VAR /d 'hello world' /f
reg add "$K\\Environment" /v SGTEST_DEL /d '' /f
reg add "$K" /v LaunchArgs /d '--from-compat -dx11' /f
reg add "$K" /v Version /d win7 /f
"$WINESERVER" -w

out=$(SGTEST_DEL=inherited timeout -s KILL 120 "$WINE" 'C:\appcompat-probe.exe' launch 'C:\appcompat-probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'VAR=hello world' && pass "its environment variable is set when it starts" || fail "Environment: $(printf '%s\n' "$out" | grep '^VAR=')"
printf '%s\n' "$out" | grep -qx 'DEL=(unset)' && pass "an empty value removes an inherited variable" || fail "removal: $(printf '%s\n' "$out" | grep '^DEL=')"
printf '%s\n' "$out" | grep -q '^CMDLINE=.* report --from-compat -dx11$' && pass "its launch arguments are appended to the command line" || fail "LaunchArgs: $(printf '%s\n' "$out" | grep '^CMDLINE=')"
printf '%s\n' "$out" | grep -qx 'VERSION=6.1' && pass "it is told it runs on Windows 7 (Version)" || fail "Version: $(printf '%s\n' "$out" | grep '^VERSION=')"

# Another program gets none of it.
out=$(timeout -s KILL 120 "$WINE" 'C:\appcompat-probe.exe' launch 'C:\appcompat-admin.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -qx 'VAR=(unset)' && pass "another program is untouched" || fail "leak to another program: $(printf '%s\n' "$out" | grep '^VAR=')"

# RUNASADMIN needs a user who is not elevated -- a second account on a
# shared prefix -- so it is gated in elevreq-gate.sh.

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
