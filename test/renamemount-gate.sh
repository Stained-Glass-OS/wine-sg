#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Renames across two mounts of one filesystem are renames (7814). On a converted machine the profile
# (C:\users\<name>) is a bind mount of /home/<name>: rename() between it and the rest of C: (Program Files,
# ProgramData), or Z:\home\<name>, fails with EXDEV even on one disk. MoveFileEx without
# MOVEFILE_COPY_ALLOWED then failed with ERROR_NOT_SAME_DEVICE (Rust's fs::rename, Python's os.replace,
# SetFileInformationByHandle), and with it copied the whole file; a folder could not move at all. On
# Windows C: is one volume and all of these are renames. Now the rename is made from the source's path as
# seen through the target's mount. In a user and mount namespace (unshare -Urm) the gate bind-mounts a
# folder into the prefix as C:\users\bound and checks, each by the inode staying the same:
#  - a file from the profile to C:\target (MoveFileEx, no flags), and back;
#  - a folder from the profile to C:\target;
#  - SetFileInformationByHandle(FileRenameInfo) from the profile to C:\target;
#  - from the profile to the same folder through Z: (another mount of it);
#  - (7816) a file on another filesystem on the same drive letter (a tmpfs inside C:, as a home on its own
#    partition is): copied even without MOVEFILE_COPY_ALLOWED, as Windows renames within one volume;
#  - a file on another drive letter (Q:, a tmpfs): ERROR_NOT_SAME_DEVICE without MOVEFILE_COPY_ALLOWED,
#    a copy with it.
#   WINE=/opt/wine-sg/bin/wine test/renamemount-gate.sh   (mutants SG_MUTANT_RENAME_NO_MOUNT_PATH: build server
#   and ntdll with it, include/wine/sg_mountpath.h; SG_MUTANT_MOVE_SAME_DRIVE_REFUSED, kernelbase file.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unshare -Urm true 2>/dev/null || { echo "SKIP: no user namespaces (unshare -Urm)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-renamemount.XXXXXX)
cleanup() { rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/renamemount-probe.c" \
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
H="$T/home"
mkdir -p "$H/dir/sub" "$C/users/bound" "$C/target" "$C/tmpfs" "$T/elsewhere"
mount --bind "$H" "$C/users/bound" || { echo "SKIP: bind mount refused"; exit 77; }
mount -t tmpfs -o size=16m tmpfs "$C/tmpfs" || { echo "SKIP: tmpfs mount refused"; exit 77; }
mkdir -p "$T/qdrive"; mount -t tmpfs -o size=16m tmpfs "$T/qdrive" || { echo "SKIP: tmpfs mount refused"; exit 77; }
ln -sfn "$T/qdrive" "$WINEPREFIX/dosdevices/q:"
echo a > "$H/a.bin"; echo b > "$C/target/b.bin"; echo c > "$H/c.bin"; echo d > "$H/d.bin"
echo t > "$C/tmpfs/t.bin"; echo u > "$T/qdrive/u.bin"; echo v > "$T/qdrive/v.bin"; echo x > "$H/dir/sub/x"
ia=$(stat -c %i "$H/a.bin"); ib=$(stat -c %i "$C/target/b.bin"); ic=$(stat -c %i "$H/c.bin")
id=$(stat -c %i "$H/d.bin"); idir=$(stat -c %i "$H/dir"); iu=$(stat -c %i "$T/qdrive/u.bin"); it=$(stat -c %i "$C/tmpfs/t.bin")
Z=$(printf 'Z:%s' "$T/elsewhere" | tr / '\\')
cp "$T/probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' \
    move 'C:\users\bound\a.bin' 'C:\target\a.bin' \
    move 'C:\target\b.bin' 'C:\users\bound\b.bin' \
    move 'C:\users\bound\dir' 'C:\target\dir' \
    info 'C:\users\bound\c.bin' 'C:\target\c.bin' \
    move 'C:\users\bound\d.bin' "$Z\\d.bin" \
    move 'C:\tmpfs\t.bin' 'C:\target\t.bin' \
    copyok 'Q:\u.bin' 'C:\target\u.bin' \
    move 'Q:\v.bin' 'C:\target\v.bin' 2>/dev/null | tr -d '\r')
echo "  probe: $(echo $out)"
set -- $out
check() {   # check RESULT PATH INODE WHAT
    if [ "$1" != OK ]; then fail "$4: the move failed ($1)"
    elif [ "$(stat -c %i "$2" 2>/dev/null)" != "$3" ]; then fail "$4: moved by a copy (another inode)"
    else pass "$4: a rename (the same inode)"; fi
}
check "${1:-none}" "$C/target/a.bin" "$ia" "a file from the profile to C:\\target"
check "${2:-none}" "$H/b.bin" "$ib" "a file from C:\\target to the profile"
check "${3:-none}" "$C/target/dir" "$idir" "a folder from the profile to C:\\target"
[ -f "$C/target/dir/sub/x" ] && pass "the folder's contents moved with it" || fail "the folder's contents are not in C:\\target\\dir"
check "${4:-none}" "$C/target/c.bin" "$ic" "SetFileInformationByHandle(FileRenameInfo) from the profile"
check "${5:-none}" "$T/elsewhere/d.bin" "$id" "from the profile to Z: (another mount of the same disk)"
if [ "${6:-none}" = OK ] && [ -f "$C/target/t.bin" ] && [ ! -e "$C/tmpfs/t.bin" ] && [ "$(stat -c %i "$C/target/t.bin")" != "$it" ]; then
    pass "another filesystem on the same drive letter, no MOVEFILE_COPY_ALLOWED: copied (7816)"
else fail "another filesystem on the same drive letter, no MOVEFILE_COPY_ALLOWED: wanted a copy, got ${6:-none}"; fi
if [ "${7:-none}" = OK ] && [ -f "$C/target/u.bin" ] && [ ! -e "$T/qdrive/u.bin" ] && [ "$(stat -c %i "$C/target/u.bin")" != "$iu" ]; then
    pass "another drive with MOVEFILE_COPY_ALLOWED: copied"
else fail "another drive with MOVEFILE_COPY_ALLOWED: wanted a copy, got ${7:-none}"; fi
[ "${8:-none}" = 17 ] && [ -f "$T/qdrive/v.bin" ] && pass "another drive without MOVEFILE_COPY_ALLOWED: ERROR_NOT_SAME_DEVICE, the file stays" \
    || fail "another drive without MOVEFILE_COPY_ALLOWED: wanted 17 and the file kept, got ${8:-none}"
"$WINESERVER" -k 2>/dev/null
umount "$C/tmpfs" "$C/users/bound" "$T/qdrive" 2>/dev/null
[ "$fails" = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL ($fails)"; exit 1
EOS
unshare -Urm sh "$T/inner.sh"
