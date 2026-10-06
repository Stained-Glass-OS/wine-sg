#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Properties sheets through SHObjectProperties (patches/sg/1175): it was a
# stub, so File Explorer's Properties for a drive in This PC, for search
# results and for the folder it shows did nothing. Now it shows the sheet on
# a thread of its own (the caller goes on): a file's, a folder's, several
# files', and a drive's -- a General page of its own (type, file system,
# used and free space with their bytes, capacity, a chart, Disk Clean-up),
# titled "Local Disk (C:) Properties"; sheets are titled "NAME Properties".
#
#   WINE=/opt/wine-sg/bin/wine test/objprops-gate.sh
#   mutants: SG_MUTANT_NO_OBJECT_PROPERTIES (shell32/shellord.c),
#            SG_MUTANT_NO_DRIVE_PAGE (shell32/shlview_cmenu.c)
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
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-objprops.XXXXXX)
n=200; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$n" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER \
       WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O1 -municode -o "$T/objprops-probe.exe" "$HERE/objprops-probe.c" -lshell32 -lole32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/objprops-probe.exe" "$WINEPREFIX/drive_c/"
probe() { timeout -s KILL 60 "$WINE" 'C:\objprops-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }
has() { printf '%s\n' "$1" | grep -qx "$2"; }

out=$(probe 'C:\windows\win.ini')
if has "$out" 'sheet=win.ini Properties' && has "$out" 'text=File type:' && has "$out" 'text=Size:'; then
    pass "a file's sheet, titled as Windows titles it"
else fail "file: $(printf '%s' "$out" | tr '\n' ' ' | head -c 400)"; fi
ms=$(printf '%s\n' "$out" | sed -n 's/^returned=1 ms=\([0-9]*\)$/\1/p')
[ -n "$ms" ] && [ "$ms" -lt 1000 ] && pass "SHObjectProperties returns at once (the sheet has a thread of its own)" \
    || fail "returned: $(printf '%s\n' "$out" | head -1)"

out=$(probe 'C:\windows')
has "$out" 'sheet=windows Properties' && has "$out" 'text=File folder' && pass "a folder's sheet" \
    || fail "folder: $(printf '%s' "$out" | tr '\n' ' ' | head -c 400)"

out=$(probe 'C:\windows\win.ini' 'C:\windows\system.ini')
printf '%s\n' "$out" | grep -q '^sheet=.*Properties' && pass "several files: a sheet" \
    || fail "several: $(printf '%s' "$out" | tr '\n' ' ' | head -c 300)"

out=$(probe 'C:\')
if has "$out" 'sheet=Local Disk (C:) Properties' && has "$out" 'text=Used space:' && has "$out" 'text=Free space:' &&
   has "$out" 'text=Capacity:' && printf '%s\n' "$out" | grep -q '^text=[0-9][0-9,.]* bytes$' &&
   has "$out" 'text=Drive C:'; then
    pass "a drive's own page: type, used and free space in bytes, capacity, titled Local Disk (C:)"
else fail "drive: $(printf '%s' "$out" | tr '\n' ' ' | head -c 600)"; fi
has "$out" 'text=Disk Clean-up' && pass "with Disk Clean-up" || fail "no Disk Clean-up button"

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
