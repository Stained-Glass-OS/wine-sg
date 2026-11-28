#!/bin/sh
# The handles a program started as another account inherits (patches/sg/9633,
# i1010). A service starts its helper on a person's desktop with
# WTSQueryUserToken's token and CreateProcessAsUser, passing pipes it names on
# the helper's command line (WireGuard's manager its tray window: "/ui 356 352
# 368 380"); the elevation broker starts such a program as the person, and it
# inherited nothing, so the helper ended at once. Now, as on Windows: with a
# PROC_THREAD_ATTRIBUTE_HANDLE_LIST the handles listed (not other inheritable
# ones), with bInheritHandles alone every inheritable handle, without it none --
# at the same values. test/runasuser-probe.c's "inherit" mode, run as SYSTEM.
# Runs INSIDE a QA VM, as root, with a person signed in:
#   SG_LI_USER=alice sh test/launchinherit-gate.sh
# SG_LI_PROBE: runasuser-probe.exe (default: built here with mingw, else beside
# this script). 77: not in a VM with a session. Mutant: SG_MUTANT_LAUNCH_NO_INHERIT
# (server/token.c; the machine's wineserver).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
U=${SG_LI_USER:?SG_LI_USER}
SYS=${SG_SYSTEM_USER:-sgsystem}
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
[ "$(id -u)" -eq 0 ] || { echo "SKIP: root only"; exit 77; }
systemd-detect-virt -q || { echo "SKIP: not in a VM"; exit 77; }
id -u "$U" >/dev/null 2>&1 || { echo "SKIP: no $U"; exit 77; }
EXP=$(pgrep -u "$U" -f 'explorer.exe' | head -1)
[ -n "$EXP" ] || { echo "SKIP: $U has no session"; exit 77; }
. /usr/lib/stained-glass/sg-common.sh
C=$SG_PREFIX/drive_c
T=$(mktemp -d /var/tmp/sg-li-gate.XXXXXX); chmod 755 "$T"
PROBE=${SG_LI_PROBE:-}
if [ -z "$PROBE" ] && command -v x86_64-w64-mingw32-gcc >/dev/null; then
    x86_64-w64-mingw32-gcc -O1 -o "$T/p.exe" "$HERE/runasuser-probe.c" -ladvapi32 -lwtsapi32 && PROBE=$T/p.exe
fi
[ -n "$PROBE" ] || PROBE=$HERE/runasuser-probe.exe
[ -f "$PROBE" ] || { echo "SKIP: no runasuser-probe.exe"; rm -rf "$T"; exit 77; }
# the probe names itself C:\runasuser-probe.exe and writes to C:\out
cleanup() { rm -rf "$T" "$C/runasuser-probe.exe" "$C/out/inherit-list" "$C/out/inherit-all" "$C/out/inherit-none"; rmdir "$C/out" 2>/dev/null; }
trap cleanup EXIT INT TERM
install -m 755 "$PROBE" "$C/runasuser-probe.exe"
mkdir -p "$C/out"; chmod 1777 "$C/out"
as_user() {
    # shellcheck disable=SC2046
    runuser -u "$U" -- env $(tr '\0' '\n' < /proc/"$EXP"/environ | grep -E '^(DISPLAY|WAYLAND_DISPLAY|XDG_RUNTIME_DIR|XAUTHORITY|HOME|USER|PATH|WINE[A-Z]*|SG_[A-Z_]*)=' | grep -v '^WINESERVERSOCKET=' | tr '\n' ' ') \
        WINEDEBUG=-all "$@"
}
as_service() {
    sh -c 'echo $$ > /sys/fs/cgroup/system.slice/sg-wineserver.service/cgroup.procs && exec runuser -u "$0" -- sh -c ". /usr/lib/stained-glass/sg-common.sh; sg_wine_env; export WINEDEBUG=-all; exec \"\$@\"" sh "$@"' "$SYS" "$@"
}
S=$(cd "$T" && as_user timeout 120 wine 'C:\runasuser-probe.exe' report 'C:\out\inherit-none' >/dev/null 2>&1; sed -n 's/^SESSION //p' "$C/out/inherit-none" | tr -d '\r')
rm -f "$C/out/inherit-none"
[ -n "$S" ] || { echo "FAIL  cannot learn $U's session"; exit 1; }
for mode in list all none; do
    out=$(cd "$T" && as_service timeout 120 wine 'C:\runasuser-probe.exe' inherit "$S" "$mode" 2>/dev/null | tr -d '\r' | grep '^INHERIT')
    owner=$(stat -c %U "$C/out/inherit-$mode" 2>/dev/null)
    case "$mode" in
    list) want="INHERIT 11 inherited:$U"; what="with a handle list the program inherits the pipe listed, not the event left out" ;;
    all)  want="INHERIT 13 inherited:$U"; what="with bInheritHandles it inherits every inheritable handle (pipe and event)" ;;
    none) want="INHERIT 10 -"; what="without bInheritHandles it inherits nothing" ;;
    esac
    [ "$out" = "$want" ] && [ "$owner" = "$U" ] && pass "$what (running as $U)" || fail "inherit $mode: '$out' (want '$want'), owner '$owner'"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
