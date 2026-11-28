#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A script stream written by the WM writer (patches/sg/9261): a profile with a script stream
# (captions, URLs, markers) could not be written at all -- BeginWriting NS_E_INVALID_STREAM. Now it
# is an ASF command media stream, as Windows writes it: a file with a script stream only (no encoder
# involved), two commands, read back (test/wmscript-probe.c).
#   WINE=/opt/wine-sg/bin/wine test/wmscript-gate.sh
# Mutant: SG_MUTANT_WMVW_NO_SCRIPT (wmvcore asfmux.c)
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
T=$(mktemp -d /var/tmp/sg-wmscript.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$T/prefix"; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/wmscript-probe.c" -lole32 -luuid || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check created          "the writer and profile manager are created"
check add_stream       "a script stream is added to a profile"
check begin            "BeginWriting with a script stream succeeds"
check write1           "a script command is written"
check write2           "a second command is written"
check end              "EndWriting succeeds"
check file             "the file is there"
check command_media    "its header has stream 1 as command media"
check commands_in_data "both commands are in the data object"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
