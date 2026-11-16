#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A move to the Recycle Bin from the user's profile is a rename (7807).
# The profile (C:\users\<name>) is a bind mount of the home, and the Trash is
# in the home (~/.local/share/Trash) through another mount: a rename between
# the two fails with EXDEV even on one disk, and the move copied the whole
# file (15 GB behind a window that said 100%). The gate makes the same two
# mounts in a user and mount namespace of its own (unshare -Urm), deletes a
# file through the bind mount and checks it reached the Trash as the same
# inode (renamed, not copied), that Restore brings it back the same way, and that a file on another filesystem (tmpfs)
# still goes to the Trash, by copying.
#
#   WINE=/opt/wine-sg/bin/wine test/trashmount-gate.sh
#   SG_MUTANT_TRASH_OTHER_MOUNT: build shell32 with it; check 1 fails
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -Urm true 2>/dev/null || { echo "SKIP: no user namespaces (unshare -Urm)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-trashmount.XXXXXX)
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/trashmount-probe.c" -lshell32 -lole32 -lshlwapi -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER T WINE HERE
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cat > "$T/inner.sh" <<'EOS'
#!/bin/sh
# in the namespace: the profile is a bind mount of the home, as on a real install
set -u
fails=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; fails=$((fails + 1)); }
mkdir -p "$T/profile" "$T/tmpfs" "$HOME/Videos"
mount --bind "$HOME" "$T/profile" || { echo "SKIP: bind mount refused"; exit 77; }
mount -t tmpfs -o size=64m tmpfs "$T/tmpfs" || { echo "SKIP: tmpfs mount refused"; exit 77; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
ln -sfn "$T" "$WINEPREFIX/dosdevices/p:"   # P:\profile is C:\users\<name>
ln -sfn "$T/tmpfs" "$WINEPREFIX/dosdevices/q:"
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
TR="$HOME/.local/share/Trash"

# 1: through the bind mount: the same inode in the Trash
head -c 33554432 /dev/zero > "$HOME/Videos/big.mp4"
ino=$(stat -c %i "$HOME/Videos/big.mp4")
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 'P:\profile\Videos\big.mp4' 2>/dev/null | tr -d '\r')
echo "  probe: $out"
if [ -e "$HOME/Videos/big.mp4" ]; then fail "the file is still in Videos"
elif [ ! -f "$TR/files/big.mp4" ]; then fail "the file is not in the Trash ($(ls "$TR/files" 2>&1))"
elif [ "$(stat -c %i "$TR/files/big.mp4")" != "$ino" ]; then
    fail "the file reached the Trash as a copy (another inode): a move between mounts copied it"
else pass "a file in the profile (a bind mount of the home) goes to the Trash by a rename (same inode)"; fi
[ -f "$TR/info/big.mp4.trashinfo" ] && grep -q "^Path=.*/Videos/big.mp4$" "$TR/info/big.mp4.trashinfo" \
    && pass "and its .trashinfo names where it was" || fail "no .trashinfo naming .../Videos/big.mp4"

# 1b: Restore puts it back the same way: a rename, the same inode
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' /restore big.mp4 2>/dev/null | tr -d '\r')
echo "  probe: $out"
if [ ! -f "$HOME/Videos/big.mp4" ]; then fail "Restore did not put the file back in Videos ($(ls "$TR/files" 2>&1))"
elif [ "$(stat -c %i "$HOME/Videos/big.mp4")" != "$ino" ]; then fail "Restore copied the file back (another inode)"
else pass "Restore puts it back by a rename (same inode)"; fi
[ ! -e "$TR/info/big.mp4.trashinfo" ] && pass "and its .trashinfo is gone" || fail "the restored file's .trashinfo is still there"

# 2: another filesystem: still to the Trash, by a copy
head -c 1048576 /dev/zero > "$T/tmpfs/other.bin"
out=$(timeout -s KILL 120 "$WINE" 'C:\probe.exe' 'Q:\other.bin' 2>/dev/null | tr -d '\r')
echo "  probe: $out"
if [ ! -e "$T/tmpfs/other.bin" ] && [ -f "$TR/files/other.bin" ] && [ "$(stat -c %s "$TR/files/other.bin")" = 1048576 ]; then
    pass "a file on another filesystem still goes to the Trash (copied, all of it)"
else fail "a file on another filesystem did not reach the Trash whole"; fi
"$WINESERVER" -k 2>/dev/null
umount "$T/tmpfs" "$T/profile" 2>/dev/null
[ "$fails" = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL ($fails)"; exit 1
EOS
unshare -Urm sh "$T/inner.sh"
