#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# cmd's FOR expands a wildcard in a quoted element (patches/sg/7005):
#   for /d %%d in ("C:\Program Files\Eclipse Adoptium\jdk-21*") do set "JAVA_HOME=%%~fd"
# found nothing (cmd searched for the name with its quotes), so scripts that look for a tool in
# a folder with spaces (JAVA_HOME, MSBuild, SDK finders) failed. The element is searched without
# its quotes and each match is quoted again. 64-bit and 32-bit (syswow64) cmd.
#   WINE=/opt/wine-sg/bin/wine test/forquoted-gate.sh
# Mutant: SG_MUTANT_FOR_QUOTED_WILDCARD (programs/cmd/wcmdmain.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-forquoted.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
mkdir -p "$C/gate dir/jdk-21.0.9" "$C/gate dir/other"
echo x > "$C/gate dir/a one.txt"; echo y > "$C/gate dir/b.txt"
printf '%s\r\n' '@echo off' \
  'for /d %%d in ("C:\gate dir\jdk-21*") do echo DIR=%%~fd' \
  'for /d %%d in ("C:\gate dir\jdk-21*") do echo RAW=%%d' \
  'for %%f in ("C:\gate dir\*.txt") do echo FILE=%%~nxf' \
  'for %%f in ("C:\gate dir\none*.zz") do echo NONE=%%f' \
  'for /d %%d in (C:\gate*) do echo PLAIN=%%d' \
  'echo END' > "$C/for.bat"
for b in 64 32; do
    cmdexe='C:\windows\system32\cmd.exe'; [ $b = 32 ] && cmdexe='C:\windows\syswow64\cmd.exe'
    out=$(timeout -s KILL 60 "$WINE" "$cmdexe" /c 'C:\for.bat' 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $b-bit: /"
    has() { printf '%s\n' "$out" | grep -qxF "$1"; }
    has 'DIR=C:\gate dir\jdk-21.0.9' && pass "$b-bit: a quoted folder pattern finds the folder (%%~fd)" || fail "$b-bit: a quoted folder pattern finds the folder"
    has 'RAW="C:\gate dir\jdk-21.0.9"' && pass "$b-bit: the match is quoted, as the element was" || fail "$b-bit: the match is quoted"
    if has 'FILE=a one.txt' && has 'FILE=b.txt'; then pass "$b-bit: a quoted file pattern finds both files"; else fail "$b-bit: a quoted file pattern finds both files"; fi
    printf '%s\n' "$out" | grep -q '^NONE=' && fail "$b-bit: a quoted pattern that matches nothing runs nothing" || pass "$b-bit: a quoted pattern that matches nothing runs nothing"
    has 'PLAIN=C:\gate dir' && pass "$b-bit: an unquoted pattern is unchanged" || fail "$b-bit: an unquoted pattern is unchanged"
    has 'END' && pass "$b-bit: the script ran to the end" || fail "$b-bit: the script ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
