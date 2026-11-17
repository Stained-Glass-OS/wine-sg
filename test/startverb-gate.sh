#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# cmd's start with a document opens it with its type's default verb, as on Windows (patches/sg/7829), and
# a first quoted argument is the console's title, not given to the program: start "" "file.docx".
# start.exe asked ShellExecute for "open": a type whose default verb is another (Edit; a suite's own)
# opened nothing ("no application associated"). Batch files in a scratch prefix, a probe as the program:
#   - start "" "C:\my doc.sgverb"        (default verb edit, no open): the probe, given edit and the file
#   - start "A title" C:\plain.sgverb    : the same, the title not among its arguments
#   - start /wait "" C:\plain.sgverb     : the same
#   - start "" C:\x.sgopen               (only an Open verb): the probe, given Open and the file
#   - start "C:\my doc.sgverb"           : a title only (a console), the probe not started
# Mutant (start.c): SG_MUTANT_START_OPEN_VERB.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-startverb.XXXXXX)
cleanup() { WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/startverb-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/probe.exe"
echo hi > "$C/my doc.sgverb"; echo hi > "$C/plain.sgverb"; echo hi > "$C/x.sgopen"
cat > "$C/assoc.reg" <<'REG'
REGEDIT4

[HKEY_CLASSES_ROOT\.sgverb]
@="SgVerb.Document"

[HKEY_CLASSES_ROOT\SgVerb.Document\shell]
@="edit"

[HKEY_CLASSES_ROOT\SgVerb.Document\shell\edit\command]
@="\"C:\\probe.exe\" edit \"%1\""

[HKEY_CLASSES_ROOT\.sgopen]
@="SgOpen.Document"

[HKEY_CLASSES_ROOT\SgOpen.Document\shell\Open\command]
@="\"C:\\probe.exe\" Open \"%1\""
REG
timeout 60 "$WINE" regedit /s 'C:\assoc.reg' >/dev/null 2>&1
n=0
case_() { # LINE WANT (a probe command line, or "none")
    n=$((n + 1))
    printf '@echo off\r\n%s\r\n' "$1" > "$C/t$n.bat"
    rm -f "$C/startverb-out.txt"
    (cd "$C" && timeout -s KILL 60 "$WINE" cmd /c "C:\\t$n.bat" >"$T/o$n" 2>&1 </dev/null)
    i=0; while [ ! -s "$C/startverb-out.txt" ] && [ $i -lt 30 ] && [ "$2" != none ]; do sleep 0.2; i=$((i + 1)); done
    [ "$2" = none ] && sleep 3
    got=$( (tr -d "\r" < "$C/startverb-out.txt") 2>/dev/null | head -1)
    if [ "$2" = none ]; then
        [ -z "$got" ] && pass "$1: a title only, nothing opened" || fail "$1: opened '$got'"
    else
        [ "$got" = "$2" ] && pass "$1: $got" || fail "$1: '$got' (want $2) $(tr -d '\r' < "$T/o$n" | head -2 | tr '\n' ' ')"
    fi
}
case_ 'start "" "C:\my doc.sgverb"' '"C:\probe.exe" edit "C:\my doc.sgverb"'
case_ 'start "A title" C:\plain.sgverb' '"C:\probe.exe" edit "C:\plain.sgverb"'
case_ 'start /wait "" C:\plain.sgverb' '"C:\probe.exe" edit "C:\plain.sgverb"'
case_ 'start "" C:\x.sgopen' '"C:\probe.exe" Open "C:\x.sgopen"'
case_ 'start "C:\my doc.sgverb"' none
"$WINESERVER" -k 2>/dev/null
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
