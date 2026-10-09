#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program's own riched20.dll (patches/sg/1522). Microsoft Office ships its
# own RICHED20.DLL (RichEdit 8) and loads it by full path; it has exports and
# interfaces the system's riched20 never had (CreateTextBoxLayout, private
# text-services interfaces). Windows loads the file the program names; Wine
# put its builtin riched20 in its place, so PowerPoint said "There's not
# enough memory or system resources" and Excel, Publisher and Outlook crashed
# on the missing pieces. riched20 now prefers a native file found that way:
#   - loaded by full path        -> the program's copy
#   - by name from the exe's dir -> the program's copy (searched first, as on Windows)
#   - by name from elsewhere     -> the system's riched20
#
#   WINE=/opt/wine-sg/bin/wine test/richedprivate-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-richedprivate.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -shared -o "$T/riched20.dll" "$HERE/richedprivate-dll.c" || { fail "dll did not build"; exit 1; }
x86_64-w64-mingw32-gcc -O2 -o "$T/probe.exe" "$HERE/richedprivate-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
A="$WINEPREFIX/drive_c/app"; O="$WINEPREFIX/drive_c/other"
mkdir -p "$A" "$O"; cp "$T/riched20.dll" "$T/probe.exe" "$A"; cp "$T/probe.exe" "$O"
R1=$(timeout 60 "$WINE" 'C:\other\probe.exe' path 'C:\app' 2>/dev/null | tr -d '\r')
R2=$(timeout 60 "$WINE" 'C:\app\probe.exe' name 2>/dev/null | tr -d '\r')
R3=$(timeout 60 "$WINE" 'C:\other\probe.exe' name 2>/dev/null | tr -d '\r')
R4=$(timeout 60 "$WINE" 'C:\app\probe.exe' system 2>/dev/null | tr -d '\r')
R5=$(timeout 60 "$WINE" 'C:\app\probe.exe' msftedit 2>/dev/null | tr -d '\r')
R6=$(timeout 60 "$WINE" 'C:\other\probe.exe' msftedit 2>/dev/null | tr -d '\r')
R7=$(timeout 60 "$WINE" 'C:\app\probe.exe' riched32 2>/dev/null | tr -d '\r')
echo "      path: $R1 / exe dir: $R2 / elsewhere: $R3 / system32 path: $R4"
echo "      msftedit beside a private copy: $R5 / elsewhere: $R6"
echo "      riched32 beside a private copy: $R7"
[ "$R1" = "private=1 system=0" ] && pass "loaded by full path, the program's own riched20.dll is used" || fail "full path: $R1"
[ "$R2" = "private=1 system=0" ] && pass "by name, the copy in the exe's directory is used" || fail "exe dir: $R2"
[ "$R3" = "private=0 system=1" ] && pass "by name elsewhere, the system's riched20 is used" || fail "elsewhere: $R3"
[ "$R4" = "private=0 system=1" ] && pass "by its system32 path, the system's riched20 is used" || fail "system32 path: $R4"
# Wine's msftedit imports riched20 (WordPad, sg-wordpad and every RICHEDIT50W
# user go through it): it must get Wine's riched20 even beside a private copy
[ "$R5" = "msftedit=1 class=1 textservices=1 riched20=system" ] &&
    pass "msftedit beside a private riched20.dll still works (Wine's riched20 under it)" || fail "msftedit beside a private copy: $R5"
[ "$R6" = "msftedit=1 class=1 textservices=1 riched20=system" ] && pass "msftedit elsewhere works" || fail "msftedit elsewhere: $R6"
# riched32 imports riched20 (its RichEdit 1.0 window procedure): Wine's own
# modules' imports of riched20 come from the system directory
[ "$R7" = "riched32=1 class=1 riched20=system" ] && pass "riched32 beside a private riched20.dll imports Wine's riched20" ||
    fail "riched32 beside a private copy: $R7"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
