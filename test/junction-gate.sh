#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Junctions (6039): a directory junction (IO_REPARSE_TAG_MOUNT_POINT) is a junction when read back,
# not a symbolic link (both are Unix symbolic links underneath; a junction's target ends in a slash).
#   mklink /J      cmd makes a junction (it said "not supported" and failed)
#   read back      FileAttributeTagInfo, FSCTL_GET_REPARSE_POINT (the mount point layout with
#                  "\??\C:\target" and "C:\target") and FindFirstFile's dwReserved0 give the junction's
#                  tag; a symbolic link (mklink /D) still gives IO_REPARSE_TAG_SYMLINK
#   use            the target's files are seen through the junction; rmdir of the junction leaves them
#   WINE=/opt/wine-sg/bin/wine test/junction-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-junction.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${KEEP:-}" ] || rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
"$MINGW" -O2 -o "$T/junction-probe.exe" "$HERE/junction-probe.c" || { fail "the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/junction-probe.exe" "$C/"
mkdir -p "$C/target"
echo hi > "$C/target/f.txt"

run() { (cd "$C" && timeout -s KILL 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}

run cmd /c 'mklink /J C:\j C:\target' >/dev/null
[ -L "$C/j" ] && pass "mklink /J makes a junction" || fail "mklink /J made no junction"
run cmd /c 'mklink /D C:\s C:\target' >/dev/null
out=$(run junction-probe.exe 'C:\j')
expect "$out" tag a0000003 "the junction's tag is IO_REPARSE_TAG_MOUNT_POINT (FileAttributeTagInfo)"
expect "$out" attr_reparse 1 "it is a reparse point"
expect "$out" fsctl a0000003 "FSCTL_GET_REPARSE_POINT gives the mount point tag"
expect "$out" subst '\??\C:\target' "with the substitute name of a junction"
expect "$out" print 'C:\target' "and its print name"
expect "$out" find a0000003 "FindFirstFile's dwReserved0 has the tag"
out=$(run junction-probe.exe 'C:\s')
expect "$out" tag a000000c "a symbolic link's tag stays IO_REPARSE_TAG_SYMLINK"
expect "$out" fsctl a000000c "and FSCTL_GET_REPARSE_POINT says so"
expect "$out" find a000000c "and FindFirstFile's dwReserved0"
out=$(run cmd /c 'dir /b C:\j')
printf '%s\n' "$out" | grep -qx 'f.txt' && pass "the target's files are seen through the junction" || fail "dir of the junction: $out"
run cmd /c 'rmdir C:\j' >/dev/null
[ ! -e "$C/j" ] && [ ! -L "$C/j" ] && [ -f "$C/target/f.txt" ] && pass "rmdir removes the junction and leaves the target's files" ||
    fail "after rmdir: $(ls -la "$C" | grep ' j' ; ls "$C/target")"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
