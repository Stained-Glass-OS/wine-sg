#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# GetTempPath2 gives the SYSTEM account %SystemRoot%\SystemTemp\ (patches/sg/
# 0638), as on Windows, when that folder exists; everyone else keeps their own
# temporary folder. Installers run elevated (Omaha: Chrome's and Brave's
# updaters) unpack there; Wine's GetTempPath2 was GetTempPath for everyone.
# A system prefix (.sg-system-prefix: its owner is SYSTEM) with and without
# the folder, and an ordinary prefix.
#
#   WINE=/opt/wine-sg/bin/wine test/systemtemp-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-systemtemp.XXXXXX)
export WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap 'WINEPREFIX="$T/user" "$WINESERVER" -k 2>/dev/null; WINEPREFIX="$T/system" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -o "$T/st.exe" "$HERE/systemtemp-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
v() { printf '%s\n' "$1" | sed -n "s/^$2=//p"; }
run() { # PREFIX
    WINEPREFIX="$1" timeout 60 "$WINE" 'C:\st.exe' 2>/dev/null | tr -d '\r'
}
for p in user system; do
    mkdir -p "$T/$p"
    [ $p = system ] && touch "$T/$p/.sg-system-prefix"
    WINEPREFIX="$T/$p" timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
    WINEPREFIX="$T/$p" "$WINESERVER" -w
    cp "$T/st.exe" "$T/$p/drive_c/"
done

out=$(run "$T/user")
[ "$(v "$out" user)" != S-1-5-18 ] && [ "$(v "$out" temp2)" = "$(v "$out" temp)" ] &&
    pass "an ordinary user's GetTempPath2 is its GetTempPath ($(v "$out" temp2))" ||
    fail "user: $(printf '%s' "$out" | tr '\n' ' ')"
out=$(run "$T/system")
[ "$(v "$out" user)" = S-1-5-18 ] || fail "the system prefix's owner is not SYSTEM: $(v "$out" user)"
[ "$(v "$out" temp2)" = "$(v "$out" temp)" ] &&
    pass "SYSTEM without SystemTemp keeps its GetTempPath ($(v "$out" temp2))" ||
    fail "system without the folder: $(printf '%s' "$out" | tr '\n' ' ')"
mkdir "$T/system/drive_c/windows/SystemTemp"
out=$(run "$T/system")
case "$(v "$out" temp2)" in
    [Cc]:\\windows\\SystemTemp\\) pass "SYSTEM gets C:\\windows\\SystemTemp\\ from GetTempPath2" ;;
    *) fail "system: temp2 '$(v "$out" temp2)'" ;;
esac
[ "$(v "$out" temp)" != "$(v "$out" temp2)" ] && pass "and GetTempPath stays its own" || fail "GetTempPath changed: $(v "$out" temp)"
exit $RC
