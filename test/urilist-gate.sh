#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Files copied in Wine as text/uri-list for X programs (patches/sg/7789): a
# CF_HDROP on Wine's clipboard (a Z:\ and a C:\ file, a space and an umlaut),
# pasted by a native X program converting CLIPBOARD to text/uri-list, gives
# file:// URIs that GLib's g_filename_from_uri takes -- no escaped "/" (it
# refuses those: GTK programs pasted nothing) -- of the files' real paths
# (C: is a link), for a wide and an ANSI DROPFILES.
#
#   WINE=/opt/wine-sg/bin/wine test/urilist-gate.sh    (mutant SG_MUTANT_URI_SLASH_ESCAPED)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DISP="${URILIST_DISPLAY:-:265}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in Xvfb gcc python3; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -f /usr/include/X11/Xlib.h ] || { echo "SKIP: needs libx11-dev"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-urilist.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${XVFB:-}" ] && kill "$XVFB" 2>/dev/null
    rm -rf "$T" "$SG_GATE_HOME"
}
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/urilist-probe.exe" "$HERE/urilist-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
gcc -O2 -o "$T/reader" "$HERE/urilist-reader.c" -lX11 || { echo "FAIL  reader did not build"; exit 1; }
Xvfb "$DISP" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
export DISPLAY="$DISP"
sleep 1
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/urilist-probe.exe" "$WINEPREFIX/drive_c/"

mkdir -p "$T/z dir" "$WINEPREFIX/drive_c/users/Public"
printf 'z\n' > "$T/z dir/über z.txt"
printf 'c\n' > "$WINEPREFIX/drive_c/users/Public/c file.txt"
ZWIN="Z:$(printf '%s' "$T/z dir/über z.txt" | tr '/' '\\')"
CWIN='C:\users\Public\c file.txt'
WANT="$(readlink -f "$T/z dir/über z.txt")|$(readlink -f "$WINEPREFIX/drive_c/users/Public/c file.txt")|"

# the paths as GLib reads them: an escaped "/" is refused
glib_paths() {
    python3 -I -c '
import sys, urllib.parse
for line in sys.stdin.buffer.read().decode().split("\r\n"):
    if not line: continue
    if not line.startswith("file:///") or "%2f" in line.lower():
        print("REFUSED:" + line); continue
    print(urllib.parse.unquote(line[7:]))
'
}

for kind in wide ansi; do
    : > "$T/set.txt"
    ( cd "$WINEPREFIX/drive_c" && "$WINE" urilist-probe.exe $kind 20 "$ZWIN" "$CWIN" > "$T/set.txt" 2>/dev/null ) &
    PROBE=$!
    for i in $(seq 100); do grep -q SET "$T/set.txt" && break; sleep 0.2; done
    sleep 1
    timeout 20 "$T/reader" > "$T/paste.txt" 2>"$T/reader.err"
    GOT=$(glib_paths < "$T/paste.txt" | tr '\n' '|')
    # an ANSI list cannot hold the umlaut in every code page: only the C: file is required then
    if [ "$GOT" = "$WANT" ] || { [ $kind = ansi ] && echo "$GOT" | grep -q "|$(readlink -f "$WINEPREFIX/drive_c/users/Public/c file.txt")|\$" && ! echo "$GOT" | grep -q REFUSED; }; then
        pass "$kind: pasted as file:// URIs GLib takes ($GOT)"
    else fail "$kind: got '$GOT' want '$WANT' (raw: $(cat "$T/paste.txt" "$T/reader.err" 2>/dev/null))"; fi
    kill "$PROBE" 2>/dev/null; wait "$PROBE" 2>/dev/null
done

[ "$RC" = 0 ] && echo "urilist: all checks passed"
exit $RC
