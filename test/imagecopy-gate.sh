#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An image's section copy (wine-sg 7750). The server copies an image's sections once per file when one
# is not page-aligned in the file (most MinGW and Electron DLLs) or is shared and writable.
# - The copy goes in the prefix, which is on disk; it used to go in the server dir, usually tmpfs /tmp,
#   where it stayed in memory.
# - When the copy cannot be written (a full /tmp or disk), the image still loads, mapped privately.
#   It used to fail with STATUS_INVALID_FILE_FOR_SECTION: WindowsCodecs.dll in the Visual Studio
#   Installer (no text, "ResourceDictionary.DeferrableContent threw") and the Google Earth installer
#   (ERROR_BAD_EXE_FORMAT).
# test/imagecopy-probe.c builds the DLL (unaligned .text, a shared writable .shared) and the program.
# "Cannot be written" is made by taking write permission away from the server dir and the prefix.
#   WINE=/opt/wine-sg/bin/wine test/imagecopy-gate.sh
# Mutants (server/mapping.c): SG_MUTANT_IMAGE_COPY_NO_FALLBACK, SG_MUTANT_IMAGE_COPY_IN_TMP.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
CC=${CC:-x86_64-w64-mingw32-gcc}
OBJCOPY=${OBJCOPY:-x86_64-w64-mingw32-objcopy}
command -v "$CC" >/dev/null && command -v "$OBJCOPY" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ "$(id -u)" = 0 ] && { echo "SKIP: root ignores the permissions this gate takes away"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-imagecopy.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
SDIR=
cleanup() { [ -n "$SDIR" ] && chmod 700 "$SDIR" 2>/dev/null; chmod 755 "$WINEPREFIX" 2>/dev/null
            "$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$CC" -O1 -DPROBE_DLL -c -o "$T/d.o" "$HERE/imagecopy-probe.c" &&
    "$OBJCOPY" --set-section-flags .shared=alloc,load,data,share "$T/d.o" &&
    TMPDIR=/var/tmp "$CC" -shared -o "$T/probe.dll" "$T/d.o" &&
    TMPDIR=/var/tmp "$CC" -municode -O1 -o "$T/imagecopy-probe.exe" "$HERE/imagecopy-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for n in a b c; do cp "$T/probe.dll" "$WINEPREFIX/drive_c/probe-$n.dll"; done
cp "$T/imagecopy-probe.exe" "$WINEPREFIX/drive_c/"
"$WINESERVER" -p30 &
sleep 1
# the server dir: <tmp>/.wine-<uid>/server-<dev>-<ino> of the prefix's config dir
SDIR=$(python3 -c 'import os,sys; s=os.stat(sys.argv[1]); print("server-%x-%x" % (s.st_dev, s.st_ino))' "$WINEPREFIX")
SDIR=$(ls -d "${TMPDIR:-/tmp}/.wine-$(id -u)/$SDIR" /tmp/.wine-$(id -u)/$SDIR 2>/dev/null | head -n1)
[ -d "$SDIR" ] || { fail "server dir not found"; exit 1; }
SPID=
for p in $(pgrep -u "$(id -u)" -x wineserver); do
    [ "$(readlink /proc/$p/cwd)" = "$SDIR" ] && SPID=$p
done
[ -n "$SPID" ] || { fail "the prefix's wineserver not found"; exit 1; }
run() { timeout -s KILL 60 "$WINE" 'C:\imagecopy-probe.exe' "$@" 2>/dev/null | tr -d '\r'; }

# 1. the copy is made, in the prefix, and the shared section is shared between processes
run 'C:\probe-a.dll' set 42 15 > "$T/set.out" &
sleep 6
copies=$(for f in /proc/$SPID/fd/*; do readlink "$f"; done | grep 'tmpmap-' | grep -c "^$WINEPREFIX/")
got=$(run 'C:\probe-a.dll' get)
wait
grep -qx 'set=42' "$T/set.out" && [ "$got" = 'get=42' ] \
    && pass "the shared writable section is shared between two processes (42)" \
    || fail "the shared section ($(cat "$T/set.out") / $got)"
[ "$copies" -ge 1 ] && pass "the section copy is in the prefix ($copies), not the server dir" \
    || fail "no section copy in the prefix ($SDIR has the server's)"
out=$(run 'C:\probe-b.dll' load)
printf '%s\n' "$out" | grep -qx 'load=ok' && pass "an unaligned image loads and runs from its copy" || fail "load with room: $out"

# 2. no room for a copy: the image loads anyway, privately
chmod 500 "$SDIR" "$WINEPREFIX"
out=$(run 'C:\probe-c.dll' load)
chmod 700 "$SDIR"; chmod 755 "$WINEPREFIX"
echo "$out"
printf '%s\n' "$out" | grep -qx 'load=ok' && pass "with no room for the copy the image still loads, its sections private" \
    || fail "with no room for the copy: $out"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
