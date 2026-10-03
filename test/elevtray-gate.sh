#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An elevated program's tray icon is shown, and works (patches/sg/0771).
#
# An elevated program runs on a Wine desktop of its own,
# WinSta0\sg-elevated-<display> (sg-elevated-run); that desktop's explorer
# made a tray no one sees, and the program's icons went there -- AmbirScan
# run as administrator had no icon (David 2026-10-02). They now go to the
# shell's tray on another desktop of the window station. The shell, below
# the elevated program, may send it the icon's callbacks: Shell_NotifyIcon
# lets that one message through UIPI (ChangeWindowMessageFilterEx, wine-sg
# 0291's server reads it), and nothing else -- a program below cannot let a
# message through for itself.
#
# elevtray-probe runs a child (this prefix's token: high integrity) on the
# desktop sg-elevated-9 with a tray icon; an observer on the shell's desktop
# finds the icon in the shell's tray, and a sender at medium integrity posts
# to the child. Mutants: SG_MUTANT_ELEVATED_OWN_TRAY (shell32: the icon in
# the hidden tray), SG_MUTANT_UIPI_NO_FILTER (server: the callback refused),
# SG_MUTANT_UIPI_ALLOW_ANYONE (server: the sender lets a message through).
#
#   WINE=/opt/wine-sg/bin/wine test/elevtray-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-elevtray.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/elevtray-probe.exe" "$HERE/elevtray-probe.c" -lshell32 -luser32 -ladvapi32 \
    || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/elevtray-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > /dev/null 2>&1 &
sleep 6
"$WINE" elevtray-probe.exe parent sg-elevated-9 25 > "$T/parent.out" 2>&1 &
i=0; while [ \$i -lt 40 ] && [ ! -s "$WINEPREFIX/drive_c/elevtray.hwnd" ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
"$WINE" elevtray-probe.exe observer > "$T/observer.out" 2>&1
sleep 2
cp "$WINEPREFIX/drive_c/elevtray.log" "$T/child.log" 2>/dev/null
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > "$T/session.out" 2>&1
o() { tr -d '\r' < "$T/observer.out" 2>/dev/null | sed -n "s/^$1 //p"; }
echo "      child: $(tr '\n' ' ' < "$T/child.log" 2>/dev/null) parent: $(cat "$T/parent.out" 2>/dev/null)"
grep -q '^add 1' "$T/child.log" 2>/dev/null && pass "the elevated program's icon is added" || fail "no icon added: $(cat "$T/child.log" 2>/dev/null)"
[ "$(o in-shell-tray)" = 1 ] && pass "and it is in the shell's tray, on the shell's desktop" \
    || fail "the icon is not in the shell's tray (it went to its own desktop's hidden one): $(cat "$T/observer.out" 2>/dev/null)"
[ "$(o callback-posted)" = 1 ] && grep -q '^callback 0202' "$T/child.log" 2>/dev/null \
    && pass "a click on it reaches the elevated program (its callback through UIPI)" \
    || fail "the callback from below was refused: posted '$(o callback-posted)', got: $(tr '\n' ' ' < "$T/child.log" 2>/dev/null)"
[ "$(o other-posted)" = 0 ] && pass "another message from below is still refused" || fail "UIPI let another message through: $(o other-posted)"
[ "$(o allow-self)" = 0 ] && [ "$(o other-posted-after)" = 0 ] && ! grep -q '^other' "$T/child.log" 2>/dev/null \
    && pass "a program below cannot let a message through for itself" \
    || fail "the program below let itself through: allow $(o allow-self), posted $(o other-posted-after)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
