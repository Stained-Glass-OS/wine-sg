#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Console programs started with no terminal (patches/sg/0403): from a
# script, a service, a scheduled task, output redirected to a file. On Windows
# every console program has a console; Wine gave these none, so a program
# that opens CONOUT$ failed -- PowerShell 7's console host stopped with a
# NullReferenceException and ran nothing (`pwsh -c ... > log` printed nothing
# and exited 0). Now such a program gets a headless console when it opens
# CONIN$/CONOUT$, keeping its redirected standard handles.
#
#   WINE=/opt/wine-sg/bin/wine test/shellconsole-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shellconsole.XXXXXX)
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" XDG_DESKTOP_DIR="$T/home/Desktop"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/shellconsole-probe.exe" "$HERE/shellconsole-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/shellconsole-probe.exe" "$WINEPREFIX/drive_c/"

# no terminal anywhere: stdin from /dev/null, stdout to a file (setsid: no
# controlling terminal either)
setsid timeout -s KILL 120 "$WINE" 'C:\shellconsole-probe.exe' < /dev/null > "$T/out" 2>/dev/null; rc=$?
out=$(tr -d '\r' < "$T/out"); echo "$out"
case "$out" in *conout=yes*) pass "a console program with no terminal can open CONOUT\$" ;; *) fail "CONOUT\$: $out" ;; esac
case "$out" in *"stdout-is-console=0"*) pass "its redirected standard output stays the file" ;; *) fail "stdout: $out" ;; esac
case "$out" in *screen-buffer=1*) pass "and its console answers (a screen buffer)" ;; *) fail "screen buffer: $out" ;; esac
[ $rc = 3 ] && pass "its exit code comes through" || fail "exit code $rc"
# and through cmd, as a logon or startup script runs it
setsid timeout -s KILL 120 "$WINE" cmd /c 'C:\shellconsole-probe.exe' < /dev/null > "$T/out2" 2>/dev/null
case "$(tr -d '\r' < "$T/out2")" in *conout=yes*) pass "so does one started by cmd with no terminal" ;; *) fail "via cmd: $(cat "$T/out2")" ;; esac
exit $RC
