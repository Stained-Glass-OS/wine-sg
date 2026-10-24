#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Unix pipe as a standard handle (6094). A program started from a Linux shell with its output piped got a pipe
# handle that GetFileType calls a pipe, but SetNamedPipeHandleState failed with ERROR_INVALID_HANDLE and
# GetNamedPipeHandleState/GetNamedPipeInfo with ERROR_INVALID_FUNCTION; a Windows anonymous pipe answers them
# (it is a named pipe). libuv gives up on such a handle: Node.js and npm died with "Error: open EBADF" writing
# to stdout or stderr. Now a Unix pipe or socket answers as a connected byte-mode pipe that blocks.
#   WINE=/opt/wine-sg/bin/wine test/unixpipe-gate.sh   (mutant SG_MUTANT_UNIX_PIPE_INFO)
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
T=$(mktemp -d /var/tmp/sg-unixpipe.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/unixpipe-probe.c" 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
# its standard error is a Unix pipe
timeout -s KILL 120 "$WINE" 'C:\probe.exe' 'C:\out.txt' 2>&1 </dev/null | cat >/dev/null
out=$(tr -d '\r' < "$WINEPREFIX/drive_c/out.txt" 2>/dev/null)
printf '%s\n' "$out" | sed 's/^/      /'
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" | cut -d= -f2- || echo none)'"; fi
}
expect type 3 "GetFileType: FILE_TYPE_PIPE"
expect set_byte_wait "1 0" "SetNamedPipeHandleState(byte, wait) succeeds"
expect set_message 0 "message mode is refused (a Unix pipe is a byte stream)"
expect get "1 mode=0 instances=1" "GetNamedPipeHandleState: byte mode, blocking, one instance"
expect info "1 flags=0" "GetNamedPipeInfo: a byte pipe's client end"
expect done 1 "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
