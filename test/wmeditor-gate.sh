#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Windows Media metadata editor (patches/sg/6515; WMCreateEditor was a stub): an ASF file's
# content description and extended descriptors read, changed, added, deleted and flushed back,
# the data after the header untouched (test/wmeditor-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmeditor-gate.sh
# Mutants: SG_MUTANT_WMED_NO_READ, SG_MUTANT_WMED_NO_WRITE (wmvcore editor.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-wmeditor.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmeditor-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 180 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check open          "WMCreateEditor opens an ASF file"
check read_title    "the content description's Title"
check read_author   "and Author"
check read_extended "an extended descriptor (WM/AlbumTitle)"
check read_types    "DWORD and BOOL descriptors with their types"
check open_twice    "a second Open while open fails"
check flush         "Flush writes the changes"
check reopen        "the file opens again"
check saved_title   "a changed Title is kept"
check saved_new     "an added attribute is kept"
check saved_kept    "the others are kept, a changed DWORD too"
check saved_deleted "a deleted attribute is gone"
check close_drops   "Close without Flush drops the changes"
check payload_kept  "the data after the header is the same"
check file_size     "the file properties carry the new file size"
check not_asf       "a file that is not ASF does not open"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
