#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A folder made where the users may make things -- under a folder whose
# default ACL gives the group all rights (ProgramData, C:\, Public) -- keeps
# the group's write whatever descriptor the program gives it (patches/sg/0769):
# a SYSTEM service's or installer's folder became 0755, its ACL mask r-x, and
# a user's program could make nothing in it (the Ambir scanner's calibration
# folder under ProgramData\AmbirTechnology: "a read write error", David
# 2026-10-02). Under a folder whose default ACL gives the group r-x (Program
# Files) the descriptor's mode stands.
#   - CreateDirectory with a descriptor giving the users read only (mode
#     0755), by a process with umask 022 (a SYSTEM service's)
#
#   WINE=/opt/wine-sg/bin/wine test/dirshare-gate.sh
# Mutations: the server with -DSG_MUTANT_DIR_MODE_NARROWS, or ntdll with
# -DSG_MUTANT_UMASK_ALWAYS: the folder's group has r-x.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
command -v setfacl >/dev/null && command -v getfacl >/dev/null || { echo "SKIP: needs acl"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-dirshare.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -municode -O2 -o "$T/probe.exe" "$HERE/dirshare-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/shared" "$C/kept"
setfacl -m d:u::rwx -m d:g::rwx -m d:o::r-x "$C/shared" 2>/dev/null || { echo "SKIP: no ACLs on this file system"; exit 77; }
setfacl -m d:u::rwx -m d:g::r-x -m d:o::r-x "$C/kept"
p() { ( umask 022; "$WINE" "$T/probe.exe" "$@" 2>/dev/null | tr -d '\r' ); }   # 022: a SYSTEM service's
mask() { getfacl -p "$1" 2>/dev/null | awk -F: '$1 == "mask" { print $3 } $1 == "group" && $2 == "" && !m { g = $3 } END { if (!m) print "" }' | head -1; }
gperm() { stat -c %A "$1" | cut -c5-7; }
r1=$(p mkdir 'C:\shared\Vendor'); r3=$(p mkdir 'C:\kept\Vendor')
"$WINESERVER" -w
[ "$r1" = ok ] && [ "$(gperm "$C/shared/Vendor")" = rwx ] \
    && pass "made with a read-only-for-users descriptor where the users may make things: the group keeps rwx ($(gperm "$C/shared/Vendor"))" \
    || fail "made in the shared folder: $r1, group $(gperm "$C/shared/Vendor" 2>&1)"
[ "$r3" = ok ] && [ "$(gperm "$C/kept/Vendor")" != rwx ] \
    && pass "under a folder giving the group r-x (Program Files): the descriptor's mode stands ($(gperm "$C/kept/Vendor"))" \
    || fail "in the kept folder: $r3, group $(gperm "$C/kept/Vendor" 2>&1)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
