#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Storage.Streams.DataReader and file: URLs for StorageFile (patches/sg/6205).
# React Native for Windows (OneDrive) opens its script bundle with GetFileFromPathAsync("file:///C:\...")
# and reads it with DataReader.FromBuffer: the URL was refused and DataReader did not exist.
#   WINE=/opt/wine-sg/bin/wine test/datareader-gate.sh
# Mutants (dlls/windows.storage): SG_MUTANT_STORAGE_NO_FILE_URL (item.c), SG_MUTANT_NO_DATA_READER,
# SG_MUTANT_READER_NO_BOUNDS (reader.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-datareader.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/datareader-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'reader_statics=00000000' "DataReader's statics"
chk 'url_backslash=00000000,1' "StorageFile.GetFileFromPathAsync takes file:///C:\\... (React Native's bundle path)"
chk 'url_slash=00000000,1' "and file:///C:/..."
chk 'from_buffer=00000000' "DataReader.FromBuffer"
chk 'left=11 big_endian=01020304' "its bytes, numbers big-endian unless set"
chk 'string=1' "strings in UTF-8, counted in code units"
chk 'little_endian=12345678 left=0' "ByteOrder little-endian"
chk 'past_end=8000000b' "reading past the end: E_BOUNDS"
chk 'from_stream=1' "a DataReader over a stream"
chk 'loaded=11 first=01020304' "LoadAsync stops at the stream's end; what it loaded reads"
chk 'done=1' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
