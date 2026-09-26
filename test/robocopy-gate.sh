#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# robocopy (patches/sg/0406): Wine's was a stub that failed every call
# (exit 16). Deployment, logon and backup scripts and package managers
# (Scoop moves every extracted package with `robocopy SRC DST /e /move`) use
# it. Checks the copy rules and robocopy's exit-code bit mask.
#
#   WINE=/opt/wine-sg/bin/wine test/robocopy-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-robocopy.XXXXXX)
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" XDG_DESKTOP_DIR="$T/home/Desktop"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
rc_of() { timeout -s KILL 60 "$WINE" cmd /v:on /c "robocopy $* >C:\\out.txt & echo !errorlevel!" 2>/dev/null | tr -d '\r' | tail -1; }
fresh() {
    rm -rf "$C/src" "$C/dst"; mkdir -p "$C/src/sub/deep" "$C/src/empty" "$C/src/skipme"
    printf 'one' > "$C/src/a.txt"; printf 'two' > "$C/src/b.log"; printf 'three' > "$C/src/sub/c.txt"
    printf 'four' > "$C/src/sub/deep/d.txt"; printf 'x' > "$C/src/skipme/x.txt"
}

fresh
r=$(rc_of 'C:\src C:\dst')
[ "$r" = 1 ] && [ -f "$C/dst/a.txt" ] && [ ! -e "$C/dst/sub" ] && pass "files only by default (exit 1: copied)" || fail "plain copy: rc=$r"
r=$(rc_of 'C:\src C:\dst')
[ "$r" = 0 ] && pass "a second run copies nothing (exit 0)" || fail "second run: rc=$r"
fresh
r=$(rc_of 'C:\src C:\dst /S')
[ "$r" = 1 ] && [ -f "$C/dst/sub/deep/d.txt" ] && [ ! -e "$C/dst/empty" ] && pass "/S copies the tree, not empty directories" || fail "/S: rc=$r"
fresh
r=$(rc_of 'C:\src C:\dst /E /XF *.log /XD skipme')
[ "$r" = 1 ] && [ -d "$C/dst/empty" ] && [ ! -e "$C/dst/b.log" ] && [ ! -e "$C/dst/skipme" ] \
    && pass "/E with empty directories; /XF and /XD exclude" || fail "/E /XF /XD: rc=$r"
fresh
r=$(rc_of 'C:\src C:\dst /E /MOVE')
[ "$r" = 1 ] && [ -f "$C/dst/sub/deep/d.txt" ] && [ ! -e "$C/src" ] && pass "/MOVE moves the tree (Scoop's way)" || fail "/MOVE: rc=$r"
fresh
timeout -s KILL 60 "$WINE" robocopy 'C:\src' 'C:\dst' /E >/dev/null 2>&1
mkdir -p "$C/dst/olddir"; printf 'extra' > "$C/dst/extra.txt"; rm -f "$C/src/a.txt"
r=$(rc_of 'C:\src C:\dst /MIR')
[ "$r" = 2 ] && [ ! -e "$C/dst/extra.txt" ] && [ ! -e "$C/dst/olddir" ] && [ ! -e "$C/dst/a.txt" ] \
    && pass "/MIR purges what the source no longer has (exit 2: extras)" || fail "/MIR: rc=$r"
fresh
printf 'changed!' > "$C/src/a.txt"; mkdir -p "$C/dst"; printf 'one' > "$C/dst/a.txt"
r=$(rc_of 'C:\src C:\dst a.txt')
[ "$r" = 1 ] && [ "$(cat "$C/dst/a.txt")" = 'changed!' ] && pass "a changed file is copied (named files only)" || fail "changed: rc=$r"
fresh
r=$(rc_of 'C:\src C:\dst /E /L')
[ "$r" = 1 ] && [ ! -e "$C/dst" ] && grep -q 'a.txt' "$C/out.txt" && pass "/L lists and copies nothing" || fail "/L: rc=$r"
r=$(rc_of 'C:\nosuchdir C:\dst')
[ "$r" = 16 ] && pass "a missing source is fatal (16)" || fail "missing source: rc=$r"
r=$(rc_of 'C:\src C:\dst /bogus')
[ "$r" = 16 ] && pass "an unknown option is fatal (16)" || fail "bad option: rc=$r"
exit $RC
