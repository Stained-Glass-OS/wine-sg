#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE server/ntdll (5927): a symbolic link opened as itself (FILE_OPEN_REPARSE_POINT)
# is deleted as itself -- relative and absolute links to a file and to a folder,
# live and dangling, through DeleteFile, RemoveDirectory and FILE_DELETE_ON_CLOSE
# (CreateFile and NtCreateFile), 64 and 32-bit. A dangling link could not be
# deleted. Each link must be gone, a live target intact, a missing one still missing.
#
#   WINE=/opt/wine-sg/bin/wine test/linkdel-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_DANGLING_LINK_FOLLOWS (dlls/ntdll/unix/file.c, server/fd.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linkdel.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T" "${SG_GATE_HOME:-$T}"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/linkdel-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/linkdel-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
RC=0
for p in p64 p32; do
    dir="$WINEPREFIX/drive_c/linkdel-$p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" "C:\\linkdel-$p" 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
    # the Unix side: no link left, the targets as they were
    left=$(find "$dir" -type l 2>/dev/null)
    [ -z "$left" ] && echo "ok   $p no link left" || { echo "FAIL $p links left:"; echo "$left"; RC=1; }
    if [ "$(cat "$dir/data.txt" 2>/dev/null)" = data ] && [ "$(cat "$dir/dir/inner.txt" 2>/dev/null)" = data ] &&
       [ ! -e "$dir/missing.txt" ] && [ ! -e "$dir/missingdir" ]; then
        echo "ok   $p targets as they were"
    else
        echo "FAIL $p targets changed"; ls -la "$dir" "$dir/dir" 2>&1; RC=1
    fi
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
