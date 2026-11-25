#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Capped (2026-11-24: an earlier draft of this gate, on a Wine whose sandboxed desktop could not be
# made, left 13,370 explorer.exe processes that filled the host's memory and swap): the gate runs in
# a scope of its own -- MemoryMax 2G, TasksMax 400, at most 20 minutes -- and on exit, or when
# killed, every process in it goes, its wineserver first.
if [ -z "${SG_GATE_SCOPED:-}" ]; then
    # root (a QA VM) makes its own scope; anyone else runs it through make (test-logondesk*), whose
    # rule wraps it in a user scope with the same limits
    [ "$(id -u)" = 0 ] && systemd-run --scope -q -p MemoryMax=2G -p TasksMax=400 -p RuntimeMaxSec=1200 -- true 2>/dev/null ||
        { echo "SKIP: needs a capped scope: run it as make test-$(basename "$0" -gate.sh) (or as root)"; exit 77; }
    export SG_GATE_SCOPED=1
    exec systemd-run --scope -q -p MemoryMax=2G -p TasksMax=400 -p RuntimeMaxSec=1200 -- "$(cd "$(dirname "$0")" && pwd)/$(basename "$0")" "$@"
fi
case "$(sed -n 's/^0:://p' /proc/self/cgroup 2>/dev/null)" in
*.scope) ;; *) echo "SKIP: not in a capped scope (make test-$(basename "$0" -gate.sh))"; exit 77 ;; esac
sg_gate_cgroup_kill() {
    c=$(sed -n 's/^0:://p' /proc/self/cgroup 2>/dev/null)
    case "$c" in *.scope) ;; *) return 0 ;; esac
    [ -n "${SG_GATE_SCOPED:-}" ] && [ -r "/sys/fs/cgroup$c/cgroup.procs" ] || return 0
    for _r in 1 2 3; do
        for _p in $(cat "/sys/fs/cgroup$c/cgroup.procs"); do [ "$_p" = $$ ] || kill -9 "$_p" 2>/dev/null; done
    done
}
# A window station and its desktops have a descriptor naming the logon SID, and a sandboxed process
# is connected to its desktop with the rights it has (patches/sg/9060, 9061), in a single user's prefix
# (test/logondesk-users-gate.sh: several users on one shared prefix). Chromium's sandbox (and
# QtWebEngine's: Jellyfin Media Player, TeamSpeak 3) makes an alternate desktop with no security
# attributes and merges a denial of RESTRICTED into its DACL; in Wine that DACL was empty, so the
# merge left one denial and the renderers had no desktop ("get_desktop_window failed to create
# desktop window"). The probe (test/logondesk-probe.c):
#   WinSta0 and the desktop have a DACL; it names the token's logon SID with full access
#   a renderer-like child (every SID restricting, low integrity) on the alternate desktop: on it,
#     a desktop window, makes a window
#   a USER_LIMITED child (Users, Everyone, RESTRICTED, the logon SID) on this desktop: the same
#   a USER_LIMITED child on the alternate desktop: connected to it with the rights its own token has
#     there (MAXIMUM_ALLOWED, as Windows connects a process): RESTRICTED is denied
#     DESKTOP_CREATEWINDOW, so no window (the server's own connection had checked the parent's token)
#   WINE=/opt/wine-sg/bin/wine test/logondesk-gate.sh
#   a USER_LIMITED child alone on the alternate desktop (nobody there may make its window): it is
#     connected, returns, and explorers do not pile up (9061: one per process and desktop, and a
#     desktop's own explorer starts none)
# Mutants: SG_MUTANT_SINGLE_USER_NO_DESKTOP_SD (server/winstation.c),
#   SG_MUTANT_DESKTOP_FULL_ACCESS_ONLY (dlls/win32u/winstation.c),
#   SG_MUTANT_DESKTOP_WINDOW_NEEDS_CREATEWINDOW (server/window.c: the lone child hangs), and
#   SG_MUTANT_DESKTOP_SHELL_RESPAWNS (dlls/win32u/winstation.c) together with the two server mutants
#   above -- the 10.0-1154 case: explorers multiply to the scope's TasksMax (349 processes on qa4;
#   21 with the guard). Run mutants in a QA VM only.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
sg_gate_display || exit $?
T=$(mktemp -d /var/tmp/sg-logondesk.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"; sg_gate_cgroup_kill' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/logondesk-probe.c" -ladvapi32 -luser32 -lntdll 2>/dev/null || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
probe() { timeout -s KILL 150 "$WINE" "$T/probe.exe" "$@" 2>/dev/null | tr -d '\r'; }
child() { printf '%s\n' "$1" | sed -n 's/^CHILD //p' | head -1; }

out=$(probe info)
printf '      %s\n' $(printf '%s\n' "$out" | tr ' ' '=')
logon=$(printf '%s\n' "$out" | sed -n 's/^LOGON //p')
case "$logon" in S-1-5-5-*) pass "the token has a logon SID ($logon)" ;; *) fail "no logon SID: '$logon'" ;; esac
printf '%s\n' "$out" | grep -q "^WINSTA ACE 0 $logon 0xf037f$" && pass "WinSta0's DACL gives the logon SID full access" \
    || fail "WinSta0: $(printf '%s\n' "$out" | grep '^WINSTA' | tr '\n' ' ')"
printf '%s\n' "$out" | grep -q "^DESKTOP ACE 0 $logon 0xf01ff$" && pass "the desktop's DACL gives the logon SID full access" \
    || fail "desktop: $(printf '%s\n' "$out" | grep '^DESKTOP' | tr '\n' ' ')"

out=$(probe renderer)
c=$(child "$out")
[ "$c" = 63 ] && pass "a renderer on Chromium's alternate desktop: on it, a desktop window, makes windows, opens it itself" \
    || fail "renderer: '$c' ($(printf '%s\n' "$out" | grep -v '^SESSION' | tr '\n' ' '))"
out=$(probe limited)
c=$(child "$out")
[ "$c" = 63 ] && pass "a USER_LIMITED child on this desktop: on it, a desktop window, makes windows, opens it itself" \
    || fail "USER_LIMITED: '$c'"
out=$(probe limited-alt)
c=$(child "$out")
case "$c" in *[!0-9]*|''|????*) fail "USER_LIMITED on the alternate desktop did not run or hung: '$c'" ;;
*) [ $((c & 3)) = 3 ] && pass "a USER_LIMITED child on the alternate desktop is connected to it ($c)" \
       || fail "USER_LIMITED on the alternate desktop: not connected ($c)"
   [ $((c & 32)) = 32 ] && pass "...it can open that desktop itself" || fail "USER_LIMITED cannot open the alternate desktop ($c)"
   [ $((c & 24)) = 0 ] && pass "...with its own rights: RESTRICTED may not make windows there, as on Windows" \
       || fail "USER_LIMITED on the alternate desktop has DESKTOP_CREATEWINDOW ($c)" ;;
esac
# 9061: alone there, nobody may make the desktop's window; no explorer is started again and again
# (the most processes the gate's scope held while it ran, sampled every second)
cg=$(sed -n 's/^0:://p' /proc/self/cgroup)
probe limited-alone > "$T/alone.out" &
pid=$! np=0
while kill -0 $pid 2>/dev/null; do
    n=$(wc -l < "/sys/fs/cgroup$cg/cgroup.procs" 2>/dev/null) || { np='?'; break; }
    [ "$n" -gt "$np" ] && np=$n
    sleep 1
done
wait $pid
c=$(child "$(cat "$T/alone.out")")
case "$c" in *[!0-9]*|''|????*) fail "USER_LIMITED alone on the alternate desktop did not run or hung: '$c' ($np processes in the gate)" ;;
*) [ $((c & 3)) = 3 ] && pass "a USER_LIMITED child alone on the alternate desktop is connected to it ($c)" \
       || fail "USER_LIMITED alone: not connected ($c)" ;;
esac
[ "$np" != '?' ] && [ "$np" -lt 40 ] && pass "...and no explorers pile up (at most $np processes in the gate)" \
    || fail "processes piled up: $np in the gate"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
