#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What the installers people run first need of Wine (patches/sg/0235-0239),
# found by the compat suite's Chrome, Node.js and Java entries:
#
#  - a COM class served by a service whose name is longer than a GUID starts
#    that service, which is given its ServiceParameters (0235; Google's
#    updater: Chrome's MSI failed with 1627);
#  - SetSecurityInfo sets the DACL of a key opened for writing only (0236;
#    the updater's COM service shut down);
#  - a type-library-marshalled interface with [string] parameters is called
#    across apartments (0237; the updater's IUpdaterSystem);
#  - an internet shortcut saves while its property set is held open (0238;
#    WiX MSIs with a website link, Node.js's: error 1603);
#  - GetProcessGroupAffinity (0239; Java printed a warning four times).
#
#   WINE=/opt/wine-sg/bin/wine test/appfix-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-appfix.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -municode -O2 -o "$T/appfix-probe.exe" "$HERE/appfix-probe.c" \
    -lole32 -loleaut32 -luuid -ladvapi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/appfix-probe.exe" "$WINEPREFIX/drive_c/"

out=""
for c in service-install localservice keydacl tlbstring urlshortcut groupaffinity; do
    out="$out
$(timeout -s KILL 120 "$WINE" 'C:\appfix-probe.exe' "$c" 2>/dev/null | tr -d '\r')"
done
printf '%s\n' "$out" | sed '/^$/d; s/^/      /'
has() { printf '%s\n' "$out" | grep -q "^$1"; }
check() { if has "$1"; then pass "$2"; else fail "$2 ($(printf '%s\n' "$out" | grep "^${1%%=*}=" | head -1 || echo none))"; fi; }

check 'service_created=1'   "a test service with a name longer than a GUID is registered"
check 'localservice=1'      "CoCreateInstance of its class starts the service, which serves it"
check 'service_args=1'      "the service is given its AppID's ServiceParameters, split like a command line"
check 'keydacl=1'           "SetSecurityInfo sets the DACL of a key opened with KEY_WRITE"
check 'tlbstring=1'         "a typelib-marshalled interface with [string] parameters works across apartments"
check 'urlshortcut=1'       "an internet shortcut saves while its property set is held open"
check 'groupaffinity=1'     "GetProcessGroupAffinity: group 0, and the count needed when short"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
