#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A file below a volume device by its NT name opens, as on Windows (wine-sg 8401):
# \Device\HarddiskVolume1\users\x\settings.json. Wine gives such names itself (QueryDosDevice("C:"),
# GetFinalPathNameByHandle with VOLUME_NAME_NT, a process's image file name) but could not open
# them: System Informer writes its settings to one and said "Unable to save application settings"
# each time it closed, then started again with its first-run questions. ntdll maps the volume to
# the drive whose \??\X: link names it; \\?\GLOBALROOT\Device\... too.
# test/volpath-probe.c, 64- and 32-bit: open a file and a directory, make a folder and a file and
# read it back by its DOS name, open the NT name GetFinalPathNameByHandle gives, GLOBALROOT, a
# missing file is STATUS_OBJECT_NAME_NOT_FOUND, a volume no drive names is not taken for one.
#   WINE=/opt/wine-sg/bin/wine test/volpath-gate.sh
# Mutant: -DSG_MUTANT_NO_VOLUME_PATHS (dlls/ntdll/unix/file.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-volpath.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/volpath-probe.c" -lntdll || { fail "$a probe did not build"; exit 1; }
done
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^$1=1"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep "^$1=" | head -1 || echo none))"; fi
    }
    check open        "a file by \\Device\\HarddiskVolumeN\\windows\\win.ini (the volume QueryDosDevice gives C:)"
    check opendir     "a directory by its volume name"
    check mkdir       "a new folder made by its volume name"
    check create      "a new file written by its volume name, read back by its DOS name"
    check finalname   "the NT name GetFinalPathNameByHandle gives opens the file"
    check globalroot  "\\\\?\\GLOBALROOT\\Device\\... through CreateFile"
    check missing     "a missing file below the volume is STATUS_OBJECT_NAME_NOT_FOUND"
    check othervolume "a volume no drive names is not taken for one"
    check done        "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
