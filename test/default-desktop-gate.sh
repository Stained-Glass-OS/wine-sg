#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# WinSta0\Default is the user's desktop (patches/sg/0080).
#
# In a session, programs run on the shell's desktop ("shell"). A program that
# names WinSta0\Default for its child -- as Mozilla-derived launchers and
# updaters do; LibreOffice's first start relaunches itself that way -- must
# get that same desktop, where it can create windows, not an empty one with
# no shell ("no driver could be loaded").
#
#   WINE=/opt/wine-sg/bin/wine test/default-desktop-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-defdesk.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/default-desktop-probe.exe" "$HERE/default-desktop-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/default-desktop-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
timeout -s KILL 90 "$WINE" default-desktop-probe.exe run > "$T/out" 2>/dev/null
EOF
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a "$T/session.sh"
out=$(tr -d '\r' < "$T/out")
printf '%s\n' "$out" | sed 's/^/      /'
get() { printf '%s\n' "$out" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
[ "$(get parent)" = shell ] || fail "the parent is not on the shell's desktop: '$(get parent)' (harness)"
[ "$(get desktop)" = shell ] && pass "a child started on WinSta0\\Default lands on the user's desktop (shell)" || fail "the child is on '$(get desktop)'"
[ "$(get window)" = 1 ] && pass "and can create its window there" || fail "the child could not create a window"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
