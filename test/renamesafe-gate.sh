#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Hardening of 7814 (7817). 7814 retries a rename refused with EXDEV through one mount of the filesystem.
#  A. Only for the server's own account: another user's rename is that user's client's (ADR 0013), made with
#     that user's rights. In a user namespace with two users (unshare --map-users, the second from the
#     subordinate ids) and a shared (system) prefix, the second user moves a file from its bind-mounted
#     profile: into a folder it may write, a rename (the same inode); into a folder only the server's
#     account may write, refused, the file still where it was.
#  B. The retry acts on folders, not on names: each alternative folder is opened one component at a time
#     without following a symbolic link and must be the original folder (device and inode), then
#     renameat() between the two folder fds. On the helper itself (include/wine/sg_mountpath.h): the real
#     alternative renames; an alternative whose parent was swapped for a symbolic link to another folder
#     (holding a decoy of the same name) is refused, and neither file moves; so is a symbolic link higher
#     up, and another real folder.
#   WINE=/opt/wine-sg/bin/wine test/renamesafe-gate.sh   (mutants SG_MUTANT_RENAME_ANY_UID, server fd.c;
#   SG_MUTANT_RENAME_FOLLOWS_LINKS, include/wine/sg_mountpath.h)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" gcc setpriv; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
# the helper's header: the source tree's or an installed one
INC=
for d in "${WINE_SRC_INCLUDE:-}" "$(dirname "$WINE")/../wine-10.0/include" "$(dirname "$WINE")/../include/wine" \
         "$(dirname "$WINE")/../include"; do
    [ -n "$d" ] && [ -f "$d/wine/sg_mountpath.h" ] && { INC=$d; break; }
done
[ -n "$INC" ] || { echo "SKIP: wine/sg_mountpath.h not found (WINE_SRC_INCLUDE)"; exit 77; }
SUB=$(awk -F: -v u="$(id -un)" '$1 == u { print $2; exit }' /etc/subuid)
SUBG=$(awk -F: -v u="$(id -un)" '$1 == u { print $2; exit }' /etc/subgid)
[ -n "$SUB" ] && [ -n "$SUBG" ] || { echo "SKIP: no subordinate ids for $(id -un)"; exit 77; }
MAP="--map-users=0:$(id -u):1 --map-users=1000:$SUB:100 --map-groups=0:$(id -g):1 --map-groups=1000:$SUBG:100"
unshare $MAP -m true 2>/dev/null || { echo "SKIP: no two-user namespaces (newuidmap)"; exit 77; }
T=$(mktemp -d /var/tmp/sg-renamesafe.XXXXXX)
chmod 755 "$T"
cleanup() { rm -rf "$T" 2>/dev/null || unshare $MAP rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -municode -O1 -o "$T/probe.exe" "$HERE/renamemount-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp gcc -O1 -I"$INC" -o "$T/swap" "$HERE/renamesafe-swap.c" || { echo "FAIL  the helper test did not build"; exit 1; }
export T WINE WINESERVER
cat > "$T/inner.sh" <<'EOS'
#!/bin/sh
set -u
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
export WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY= WAYLAND_DISPLAY=sg-no-wayland
# -- B: the helper itself
R="$T/b"; mkdir -p "$R/real/home" "$R/real/other" "$R/real/target" "$R/prof"
mount --bind "$R/real/home" "$R/prof" || { echo "SKIP: bind mount refused"; exit 77; }
echo src > "$R/real/home/f"; echo decoy > "$R/real/other/f"
ln -s "$R/real/other" "$R/swap"; ln -s "$R/real" "$R/lnk"
r=$("$T/swap" "$R/prof/f" "$R/real/target/f" "$R/swap/f" "$R/real/target/f")
if [ "${r%% *}" = refused ] && [ -f "$R/real/home/f" ] && [ -f "$R/real/other/f" ] && [ ! -e "$R/real/target/f" ]; then
    pass "an alternative whose parent is a symbolic link to another folder is refused ($r); nothing moves"
else fail "an alternative through a swapped-in symbolic link: $r; target: $(ls "$R/real/target" | tr '\n' ' ')"; fi
r=$("$T/swap" "$R/prof/f" "$R/real/target/f" "$R/lnk/home/f" "$R/real/target/f")
[ "${r%% *}" = refused ] && [ -f "$R/real/home/f" ] && pass "a symbolic link higher up the alternative is refused too ($r)" \
    || fail "a symbolic link higher up: $r"
r=$("$T/swap" "$R/prof/f" "$R/real/target/f" "$R/real/other/f" "$R/real/target/f")
[ "${r%% *}" = refused ] && [ -f "$R/real/other/f" ] && pass "another real folder as the alternative is refused ($r)" \
    || fail "another real folder: $r"
ino=$(stat -c %i "$R/real/home/f")
r=$("$T/swap" "$R/prof/f" "$R/real/target/f" "$R/real/home/f" "$R/real/target/f")
[ "$r" = renamed ] && [ "$(stat -c %i "$R/real/target/f" 2>/dev/null)" = "$ino" ] \
    && pass "the true alternative renames (the same inode)" || fail "the true alternative: $r"
umount "$R/prof"
# -- A: a second user on a shared prefix
P="$T/prefix"; C="$P/drive_c"
mkdir -p "$P" "$T/h0" "$T/h1" "$T/xdg0" "$T/xdg1" "$T/home1"
chown 0:1050 "$P"; chmod 2770 "$P"; touch "$P/.sg-system-prefix"
chown 1001:1001 "$T/h1" "$T/xdg1" "$T/home1"
export WINEPREFIX="$P"
HOME="$T/h0" XDG_RUNTIME_DIR="$T/xdg0" timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
HOME="$T/h0" XDG_RUNTIME_DIR="$T/xdg0" "$WINESERVER" -p 120 &
sleep 1
mkdir -p "$C/users/bound1" "$C/target" "$C/serveronly"
chgrp 1050 "$C/target"; chmod 2775 "$C/target"; chmod 755 "$C/serveronly"
mount --bind "$T/home1" "$C/users/bound1" || { echo "SKIP: bind mount refused"; exit 77; }
setpriv --reuid=1001 --regid=1001 --groups=1050 sh -c "echo a > '$T/home1/a.bin'; echo b > '$T/home1/b.bin'"
ia=$(stat -c %i "$T/home1/a.bin")
cp "$T/probe.exe" "$C/"; chmod 755 "$C/probe.exe"
out=$(cd "$C" && setpriv --reuid=1001 --regid=1001 --groups=1050 env HOME="$T/h1" XDG_RUNTIME_DIR="$T/xdg1" \
      timeout -s KILL 120 "$WINE" 'C:\probe.exe' \
      move 'C:\users\bound1\a.bin' 'C:\target\a.bin' \
      move 'C:\users\bound1\b.bin' 'C:\serveronly\b.bin' 2>/dev/null | tr -d '\r')
echo "  second user: $(echo $out)"
set -- $out
if [ "${1:-none}" = OK ] && [ "$(stat -c %i "$C/target/a.bin" 2>/dev/null)" = "$ia" ]; then
    pass "another user's move from its profile into a folder it may write: a rename, by its own client"
else fail "another user's move into a folder it may write: ${1:-none}"; fi
if [ "${2:-none}" != OK ] && [ -f "$T/home1/b.bin" ] && [ ! -e "$C/serveronly/b.bin" ]; then
    pass "another user's move into a folder only the server's account may write is refused (${2:-none}); the server does not make it"
else fail "another user's move into the server's own folder: ${2:-none}; there: $(ls "$C/serveronly" | tr '\n' ' ')"; fi
"$WINESERVER" -k 2>/dev/null
umount "$C/users/bound1" 2>/dev/null
[ "$fails" = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL ($fails)"; exit 1
EOS
unshare $MAP -m sh "$T/inner.sh"
rc=$?
exit $rc
