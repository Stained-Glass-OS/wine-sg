#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Acceptance: the real winget, end to end, in a fresh prefix on a private X
# server (never the desktop's). winget is Microsoft's and is never shipped;
# WINGET_DIR names a copy the user supplied -- winget.exe and its DLLs, plus
# the winsqlite3/icuuc/icuin DLLs it needs beside it. Needs the network.
#
#   WINGET_DIR=/path/to/winget WINE=/opt/wine-sg/bin/wine test/winget-e2e.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ -n "${WINGET_DIR:-}" ] && [ -f "$WINGET_DIR/winget.exe" ] || { echo "SKIP: set WINGET_DIR to a winget copy"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: xvfb-run not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
curl -fsI --max-time 15 https://cdn.winget.microsoft.com/cache/source2.msix >/dev/null || { echo "SKIP: no network"; exit 77; }

T=$(mktemp -d /var/tmp/sg-winget.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp -r "$WINGET_DIR" "$WINEPREFIX/drive_c/winget"
PF="$WINEPREFIX/drive_c/Program Files"

wg() { (cd "$WINEPREFIX/drive_c/winget" && timeout -s KILL 900 xvfb-run -a "$WINE" winget.exe "$@" \
        --disable-interactivity 2>/dev/null | tr -d '\r'); }

out=$(wg search python --accept-source-agreements)
echo "$out" | grep -q '^Name *Id *Version' && pass "search: localized table headers (MRT)" || fail "search: no header row"
echo "$out" | grep -qE 'winget *$' && pass "search: the community source answers" || fail "search: no winget-source results"
echo "$out" | grep -qE 'msstore *$' && pass "search: the Store source answers" || fail "search: no msstore results"

out=$(wg show Python.Python.3.12)
echo "$out" | grep -q '^Publisher: Python Software Foundation' && pass "show: a compressed manifest decodes" || fail "show failed"

out=$(wg install --id 7zip.7zip -e --accept-package-agreements --accept-source-agreements)
echo "$out" | grep -q 'Successfully installed' && [ -f "$PF/7-Zip/7z.exe" ] && pass "install: an MSI package" || fail "install 7-Zip (MSI) failed"
out=$(wg install --id Notepad++.Notepad++ -e --accept-package-agreements --accept-source-agreements)
echo "$out" | grep -q 'Successfully installed' && [ -f "$PF/Notepad++/notepad++.exe" ] && pass "install: an NSIS .exe package" \
    || fail "install Notepad++ (NSIS) failed"

out=$(wg list)
echo "$out" | grep -q '7zip.7zip' && echo "$out" | grep -q 'Notepad++.Notepad++' && pass "list: both installed packages" || fail "list is missing a package"

out=$(wg uninstall --id 7zip.7zip -e)
echo "$out" | grep -q 'Successfully uninstalled' && [ ! -e "$PF/7-Zip/7z.exe" ] && pass "uninstall" || fail "uninstall failed"

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
