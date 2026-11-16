#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Cancel while a delete copies a file into the Recycle Bin (7807's other half, gate 7813). A file on another
# filesystem reaches the Trash only by a copy; before 7807 the progress window counted the item before the
# work (it said 100% at once) and the copy took no progress routine, so Cancel said "Cancelling..." and the
# delete finished anyway. Here the file is on a slow FUSE filesystem of the gate's own (in a user and mount
# namespace, unshare -Urm), so the copy runs for seconds; in Xvfb the gate checks that:
#  - the progress window appears, its title "NN% complete" stays under 100% and moves with the bytes;
#  - Cancel (BM_CLICK on its button) stops the copy: the operation ends aborted;
#  - the file is still there, whole; no partial file is left in Trash/files, no entry in Trash/info;
#  - no message box ("delete it permanently?") follows the Cancel.
#   WINE=/opt/wine-sg/bin/wine test/trashcancel-gate.sh
#   (mutants SG_MUTANT_TRASH_DONE_FIRST, SG_MUTANT_TRASH_NO_PROGRESS: shell32 shlfileop.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" gcc pkg-config Xvfb; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
pkg-config --exists fuse3 || { echo "SKIP: no libfuse3 development files"; exit 77; }
[ -w /dev/fuse ] || { echo "SKIP: /dev/fuse not usable"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -Urm true 2>/dev/null || { echo "SKIP: no user namespaces (unshare -Urm)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-trashcancel.XXXXXX)
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/trashcancel-probe.c" -lshell32 \
    || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp gcc -O1 -o "$T/slowfs" "$HERE/trashcancel-slowfs.c" $(pkg-config --cflags --libs fuse3) \
    || { echo "FAIL  the FUSE filesystem did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINESERVER T WINE HERE
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cat > "$T/inner.sh" <<'EOS'
#!/bin/sh
set -u
fails=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; fails=$((fails + 1)); }
mkdir -p "$T/slow"
"$T/slowfs" "$T/slow" 2>/dev/null &
FS=$!
for i in 1 2 3 4 5 6 7 8 9 10; do [ -f "$T/slow/slow.bin" ] && break; sleep 0.3; done
[ -f "$T/slow/slow.bin" ] || { echo "SKIP: the FUSE filesystem did not mount"; kill $FS; exit 77; }
Xvfb :267 -nolisten tcp -screen 0 1024x768x24 >/dev/null 2>&1 &
XV=$!
sleep 1
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY=:267 "$WINE" wineboot -i >/dev/null 2>&1
ln -sfn "$T/slow" "$WINEPREFIX/dosdevices/q:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
TR="$HOME/.local/share/Trash"
out=$(cd "$WINEPREFIX/drive_c" && DISPLAY=:267 timeout -s KILL 120 "$WINE" 'C:\probe.exe' 'Q:\slow.bin' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() { printf '%s\n' "$out" | grep -qxF "$1=$2" && pass "$3" || fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; }
expect Shown 1 "the progress window appears while the file is copied into the Recycle Bin"
expect Percent "1 1" "its title stays under 100% and moves with the bytes"
expect Running 1 "the copy is still running when Cancel is pressed"
expect Cancel 1 "Cancel pressed (BM_CLICK)"
expect Result "1 1 1" "the delete ends, cancelled (aborted)"
expect Dialog 0 "no message box (\"delete it permanently?\") follows"
size=$(stat -c %s "$T/slow/slow.bin" 2>/dev/null || echo none)
if [ "$size" = 25165824 ] && [ "$(head -c 1048576 "$T/slow/slow.bin" | tr -d S | wc -c)" = 0 ]; then
    pass "the file is still there, whole"
else fail "the file is not there whole (size $size)"; fi
left=$(ls -A "$TR/files" 2>/dev/null; ls -A "$TR/info" 2>/dev/null)
[ -z "$left" ] && pass "nothing left in Trash/files or Trash/info" || fail "left in the Trash: $left"
"$WINESERVER" -k 2>/dev/null
kill $XV 2>/dev/null
umount "$T/slow" 2>/dev/null || fusermount3 -u "$T/slow" 2>/dev/null
kill $FS 2>/dev/null
[ "$fails" = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL ($fails)"; exit 1
EOS
unshare -Urm sh "$T/inner.sh"
