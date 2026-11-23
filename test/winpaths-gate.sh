#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# fontconfig's fc-cache gets Windows' meaning of a rooted path (patches/sg/9900). Gramps' installer ran
# its fc-cache with FC_DEBUG=128 through NSIS <= 3.11 nsExec; fc-cache's default "/usr/share/fonts"
# resolved (Wine's "may be a Unix path" rule) to the Linux system's fonts folder, its cmap and encoding
# files made lines of 6000 characters, and nsExec's shrinking line buffer overran the heap: the
# installer crashed. With the probe (test/winpaths-probe.c), started in C:\:
#   as fc-cache.exe: "/usr/share" is C:\usr\share, which does not exist (Windows' meaning)
#   as another program: "/usr/share" is still Z:\usr\share (Wine's rule for Unix paths)
#   HKCU\Software\Wine\AppDefaults\fc-cache.exe SgUnixPaths=1 gives it Wine's rule back (HKCU first)
#   WINE=/opt/wine-sg/bin/wine test/winpaths-gate.sh
# Mutant: ntdll SG_MUTANT_UNIX_PATHS_ALWAYS.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
[ -d /usr/share ] || { echo "SKIP: no /usr/share"; exit 77; }
T=$(mktemp -d /var/tmp/sg-winpaths.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/winpaths.exe" "$HERE/winpaths-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
mkdir -p "$WINEPREFIX/drive_c/t"
cp "$T/winpaths.exe" "$WINEPREFIX/drive_c/t/fc-cache.exe"
cp "$T/winpaths.exe" "$WINEPREFIX/drive_c/t/other.exe"
rc=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; rc=1; }
run() { (cd "$WINEPREFIX/drive_c" && timeout 120 "$WINE" "C:\\t\\$1" /usr/share 2>/dev/null | tr -d '\r'); }
o=$(run fc-cache.exe)
[ "$o" = 'FULL C:\usr\share EXISTS 0' ] && pass "fc-cache.exe: /usr/share is C:\\usr\\share, which does not exist" \
    || fail "fc-cache.exe: '$o' (want 'FULL C:\\usr\\share EXISTS 0')"
o=$(run other.exe)
[ "$o" = 'FULL Z:\usr\share EXISTS 1' ] && pass "another program: /usr/share is Z:\\usr\\share (Wine's rule)" \
    || fail "other.exe: '$o' (want 'FULL Z:\\usr\\share EXISTS 1')"
"$WINE" reg add 'HKCU\Software\Wine\AppDefaults\fc-cache.exe' /v SgUnixPaths /t REG_DWORD /d 1 /f >/dev/null 2>&1
o=$(run fc-cache.exe)
[ "$o" = 'FULL Z:\usr\share EXISTS 1' ] && pass "HKCU SgUnixPaths=1 gives fc-cache.exe Wine's rule back" \
    || fail "fc-cache.exe with HKCU SgUnixPaths=1: '$o'"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
