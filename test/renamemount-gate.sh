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
#  - a file on another filesystem (tmpfs): ERROR_NOT_SAME_DEVICE without MOVEFILE_COPY_ALLOWED, a copy with it.
#   WINE=/opt/wine-sg/bin/wine test/renamemount-gate.sh   (mutant SG_MUTANT_RENAME_NO_MOUNT_PATH: build server
#   and ntdll with it, include/wine/sg_mountpath.h)
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
echo a > "$H/a.bin"; echo b > "$C/target/b.bin"; echo c > "$H/c.bin"; echo d > "$H/d.bin"
echo t > "$C/tmpfs/t.bin"; echo u > "$C/tmpfs/u.bin"; echo x > "$H/dir/sub/x"
ia=$(stat -c %i "$H/a.bin"); ib=$(stat -c %i "$C/target/b.bin"); ic=$(stat -c %i "$H/c.bin")
id=$(stat -c %i "$H/d.bin"); idir=$(stat -c %i "$H/dir"); iu=$(stat -c %i "$C/tmpfs/u.bin")
Z=$(printf 'Z:%s' "$T/elsewhere" | tr / '\\')
cp "$T/probe.exe" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' \
    move 'C:\users\bound\a.bin' 'C:\target\a.bin' \
    move 'C:\target\b.bin' 'C:\users\bound\b.bin' \
    move 'C:\users\bound\dir' 'C:\target\dir' \
    info 'C:\users\bound\c.bin' 'C:\target\c.bin' \
    move 'C:\users\bound\d.bin' "$Z\\d.bin" \
    move 'C:\tmpfs\t.bin' 'C:\target\t.bin' \
    copyok 'C:\tmpfs\u.bin' 'C:\target\u.bin' 2>/dev/null | tr -d '\r')
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
[ "${6:-none}" = 17 ] && [ -f "$C/tmpfs/t.bin" ] && pass "another filesystem without MOVEFILE_COPY_ALLOWED: ERROR_NOT_SAME_DEVICE, the file stays" \
    || fail "another filesystem without MOVEFILE_COPY_ALLOWED: wanted 17 and the file kept, got ${6:-none}"
if [ "${7:-none}" = OK ] && [ -f "$C/target/u.bin" ] && [ ! -e "$C/tmpfs/u.bin" ] && [ "$(stat -c %i "$C/target/u.bin")" != "$iu" ]; then
    pass "another filesystem with MOVEFILE_COPY_ALLOWED: copied"
else fail "another filesystem with MOVEFILE_COPY_ALLOWED: wanted a copy, got ${7:-none}"; fi
"$WINESERVER" -k 2>/dev/null
umount "$C/tmpfs" "$C/users/bound" 2>/dev/null
[ "$fails" = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL ($fails)"; exit 1
EOS
unshare -Urm sh "$T/inner.sh"
