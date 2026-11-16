#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder moved to another filesystem arrives there (7815). SHFileOperation(FO_MOVE) renamed a folder,
# and when the target was on another filesystem (a USB disk, a home on its own partition) the rename
# failed with ERROR_NOT_SAME_DEVICE, which went unnoticed: the operation said it was done and left the
# folder where it was. As on Windows, the folder is now made on the other side and its contents moved
# into it, then the empty source goes. In a user and mount namespace (unshare -Urm) with a tmpfs as the
# other filesystem, the gate checks:
#  - a folder moved to a new name there, and one moved into a folder there: all of it arrives (a nested
#    folder, an empty one, a hidden file), the source is gone, the operation reports success;
#  - a folder moved within one filesystem is still a rename (the same inode).
#   WINE=/opt/wine-sg/bin/wine test/shmovedir-gate.sh   (mutant SG_MUTANT_SHMOVE_NO_COPY, shell32 shlfileop.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -Urm true 2>/dev/null || { echo "SKIP: no user namespaces (unshare -Urm)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shmovedir.XXXXXX)
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/shmovedir-probe.c" -lshell32 \
    || { echo "FAIL  probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINESERVER T WINE
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cat > "$T/inner.sh" <<'EOS'
#!/bin/sh
set -u
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/usb"
mount -t tmpfs -o size=16m tmpfs "$C/usb" || { echo "SKIP: tmpfs mount refused"; exit 77; }
for d in d e same; do mkdir -p "$C/$d/sub" "$C/$d/empty"; echo y > "$C/$d/sub/y"; echo h > "$C/$d/.hidden"; done
mkdir -p "$C/usb/into" "$C/here"
isame=$(stat -c %i "$C/same")
cp "$T/probe.exe" "$C/"
run() { (cd "$C" && timeout -s KILL 60 "$WINE" 'C:\probe.exe' "$1" "$2" 2>/dev/null | tr -d '\r'); }
whole() { [ -f "$1/sub/y" ] && [ -d "$1/empty" ] && [ -f "$1/.hidden" ]; }
r=$(run 'C:\d' 'C:\usb\d'); echo "  move to a new name on the other filesystem: $r"
if [ "$r" = "ret=0 aborted=0" ] && whole "$C/usb/d" && [ ! -e "$C/d" ]; then
    pass "a folder moved to a new name on another filesystem arrives whole, the source is gone"
else fail "a folder moved to a new name on another filesystem: $r; there: $(ls -A "$C/usb/d" 2>&1 | tr '\n' ' '); source: $(ls -A "$C/d" 2>&1 | tr '\n' ' ')"; fi
r=$(run 'C:\e' 'C:\usb\into'); echo "  move into a folder on the other filesystem: $r"
if [ "$r" = "ret=0 aborted=0" ] && whole "$C/usb/into/e" && [ ! -e "$C/e" ]; then
    pass "a folder moved into a folder on another filesystem arrives whole, the source is gone"
else fail "a folder moved into a folder on another filesystem: $r; there: $(ls -A "$C/usb/into/e" 2>&1 | tr '\n' ' ')"; fi
r=$(run 'C:\same' 'C:\here'); echo "  move within one filesystem: $r"
if [ "$r" = "ret=0 aborted=0" ] && whole "$C/here/same" && [ "$(stat -c %i "$C/here/same")" = "$isame" ]; then
    pass "a folder moved within one filesystem is still a rename (the same inode)"
else fail "a folder moved within one filesystem: $r"; fi
"$WINESERVER" -k 2>/dev/null
umount "$C/usb" 2>/dev/null
[ "$fails" = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL ($fails)"; exit 1
EOS
unshare -Urm sh "$T/inner.sh"
