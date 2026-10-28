#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows Terminal's start-up (patches/sg/6174): SHGetKnownFolderPath with the package and app-data
# redirection flags (0x10000-0x80000) gives the folder as without them (it failed with E_INVALIDARG and
# the terminal's settings did not load); and HKLM CurrentVersion has ProgramW6432Dir and CommonW6432Dir
# (wine.inf), read to rebuild a new shell's environment (a missing value aborted the shell's start).
# test/knownfolderflags-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/knownfolderflags-gate.sh
# Mutant: SG_MUTANT_KF_REDIRECTION_REJECTED (dlls/shell32/shellpath.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-knownfolderflags.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/knownfolderflags-probe.c" -lshell32 -lole32 -ladvapi32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'flags 0x80000: 00000000 1' "KF_FLAG_FORCE_APP_DATA_REDIRECTION: the folder, as without it"
chk 'flags 0x40000: 00000000 1' "KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET: the same"
chk 'flags 0x20000: 00000000 1' "KF_FLAG_FORCE_PACKAGE_REDIRECTION: the same"
chk 'flags 0x10000: 00000000 1' "KF_FLAG_NO_PACKAGE_REDIRECTION: the same"
chk 'ProgramW6432Dir: 0 1' "ProgramW6432Dir is C:\\Program Files"
chk 'CommonW6432Dir: 0 1' "CommonW6432Dir is C:\\Program Files\\Common Files"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
