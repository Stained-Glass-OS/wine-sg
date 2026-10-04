#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A program by its App Paths name from the user's own App Paths, with the
# entry's own arguments first (patches/sg/0793): start firefox, Win+R gimp,
# Start-Process vlc start the Linux app (sg-shell's sg-linuxapp keeps these
# entries; David 2026-10-04: Windows programs and the command line could not
# open Linux Firefox). Windows reads HKCU's App Paths too; here the machine's
# win over the user's, so a program installed for everyone keeps its name.
#
#   WINE=/opt/wine-sg/bin/wine test/userapppaths-gate.sh   (mutant SG_MUTANT_NO_USER_APPPATHS)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-userapppaths.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/probe.exe" "$HERE/userapppaths-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
C="$WINEPREFIX/drive_c"
cp "$T/probe.exe" "$C/launcher.exe"; cp "$T/probe.exe" "$C/machine.exe"
AP='Software\Microsoft\Windows\CurrentVersion\App Paths'
timeout 60 "$WINE" reg add "HKCU\\$AP\\gatefox.exe" /ve /d 'C:\launcher.exe' /f >/dev/null 2>&1
timeout 60 "$WINE" reg add "HKCU\\$AP\\gatefox.exe" /v SgArguments /d '--launch "Z:\usr\share\applications\gatefox.desktop"' /f >/dev/null 2>&1
# the same name for everyone (a Windows program) and for the user
timeout 60 "$WINE" reg add "HKLM\\$AP\\gatetool.exe" /ve /d 'C:\machine.exe' /f >/dev/null 2>&1
timeout 60 "$WINE" reg add "HKCU\\$AP\\gatetool.exe" /ve /d 'C:\launcher.exe' /f >/dev/null 2>&1
timeout 60 "$WINE" reg add "HKCU\\$AP\\gatetool.exe" /v SgArguments /d '--launch "Z:\x\gatetool.desktop"' /f >/dev/null 2>&1
started() {   # started N: the probe's Nth line
    i=0; while [ $i -lt 40 ] && [ "$(tr -d '\r' 2>/dev/null < "$C/started.txt" | wc -l)" -lt "$1" ]; do sleep 0.25; i=$((i + 1)); done
    tr -d '\r' 2>/dev/null < "$C/started.txt" | sed -n "${1}p"
}
printf '@start gatefox https://example.org/?a=1^&b=2\r\n' > "$C/a.bat"
timeout 60 "$WINE" cmd /c 'C:\a.bat' >/dev/null 2>&1
got=$(started 1)
[ "$got" = ' --launch "Z:\usr\share\applications\gatefox.desktop" https://example.org/?a=1&b=2' ] \
    && pass "start NAME: the user's App Paths entry, its own arguments before the caller's" || fail "start gatefox: '$got'"
printf '@start "" gatefox.exe\r\n' > "$C/b.bat"
timeout 60 "$WINE" cmd /c 'C:\b.bat' >/dev/null 2>&1
got=$(started 2)
[ "$got" = ' --launch "Z:\usr\share\applications\gatefox.desktop"' ] && pass "NAME.exe, no arguments: the entry's alone" || fail "start gatefox.exe: '$got'"
printf '@start gatetool\r\n' > "$C/c.bat"
timeout 60 "$WINE" cmd /c 'C:\c.bat' >/dev/null 2>&1
started 3 >/dev/null
case "$(tr -d '\r' < "$C/started.txt" | sed -n 3p)" in *gatetool.desktop*) fail "the user's entry took a name the machine's has";;
    *) [ "$(tr -d '\r' < "$C/started.txt" | wc -l)" -ge 3 ] && pass "a name the machine's App Paths has runs the machine's program" || fail "start gatetool ran nothing";; esac
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
