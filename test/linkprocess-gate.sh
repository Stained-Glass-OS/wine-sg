#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ShellExecuteEx on a shortcut gives the process the shortcut started (6102). A .lnk is run through its
# IContextMenu, which had no way to hand back the target's process, so SEE_MASK_NOCLOSEPROCESS gave no
# hProcess: `start /wait TeXstudio.lnk` (and launchers waiting on a Start menu shortcut) waited on nothing
# and ended at once with an undefined exit code (3) while the program ran on. Windows gives the target's
# process. The probe makes a shortcut to itself ("child": 1.5 s, exit code 7) and runs it.
#   WINE=/opt/wine-sg/bin/wine test/linkprocess-gate.sh
# Mutant: SG_MUTANT_LINK_NO_PROCESS (shell32 shelllink.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v $MINGW >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-linkprocess.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
unset DISPLAY XAUTHORITY
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/linkprocess-probe.c" -lshell32 -lole32 -luuid 2>"$T/cc.log" ||
    { cat "$T/cc.log"; fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/probe.exe"
"$WINE" 'C:\probe.exe' make 'C:\child.lnk' > "$T/make.out" 2>/dev/null
grep -q 'made=0' "$T/make.out" || { cat "$T/make.out"; fail "the shortcut was not made"; echo "RESULT: FAIL"; exit 1; }
out=$(timeout 60 "$WINE" 'C:\probe.exe' run 'C:\child.lnk' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v executed)" = 1 ] && [ "$(v process)" = 1 ] && pass "ShellExecuteEx on a shortcut with SEE_MASK_NOCLOSEPROCESS gives a process" \
    || fail "no process for the shortcut (executed=$(v executed) process=$(v process))"
[ "$(v exit)" = 7 ] && [ "$(v waited)" = 1 ] && pass "it is the target's: waiting on it lasts until the program ends, exit code 7" \
    || fail "the process given is not the target's (exit=$(v exit) waited=$(v waited))"
[ "$(v unasked_executed)" = 1 ] && [ "$(v unasked_process)" = 0 ] && pass "without SEE_MASK_NOCLOSEPROCESS none is given" \
    || fail "unasked: executed=$(v unasked_executed) process=$(v unasked_process)"
s=$(date +%s)
(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" start /wait 'C:\child.lnk' >/dev/null 2>&1); rc=$?
took=$(( $(date +%s) - s ))
[ "$rc" = 7 ] && pass "start /wait on the shortcut waits for the program and gives its exit code ($rc)" \
    || fail "start /wait on the shortcut: exit code $rc after ${took}s, not 7"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
