#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Drives and discs coming and going reach File Explorer and programs
# (patches/sg/0637). Mount manager's notices go to the SYSTEM account's
# desktop, so a disc put in did not show in This PC until it was opened again,
# and one taken out stayed with its name and size (David). Now File Explorer
# looks at its drives every two seconds and reads This PC and the navigation
# pane again when they change, and the session's desktop broadcasts
# WM_DEVICECHANGE (DBT_DEVICEARRIVAL / DBT_DEVICEREMOVECOMPLETE of a volume)
# to programs. Here a drive letter is added to the prefix and taken away: This
# PC must list it, then not, and a program must hear both. (A disc in a real
# drive, and the empty drive keeping its letter -- mount manager -- are
# QA-VM checks.)
#
#   WINE=/opt/wine-sg/bin/wine test/drivewatch-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-drivewatch.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; [ -n "${SG_KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/drivewatch-probe.exe" "$HERE/drivewatch-probe.c" || { fail "probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
mkdir -p "$WINEPREFIX" "$T/rdrive"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/drivewatch-probe.exe" "$WINEPREFIX/drive_c/"
# programs join the shell's desktop, as in a session
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x700 >/dev/null 2>&1 &
sleep 6
(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" drivewatch-probe.exe 40 > "$T/probe.out" 2>/dev/null &)
WINEDEBUG=trace+explorer "$WINE" explorer.exe > /dev/null 2> "$T/fe.log" &
sleep 8
before=$(grep -c 'this pc drive' "$T/fe.log")
(cd "$WINEPREFIX/drive_c" && "$WINE" drivewatch-probe.exe define R "$T/rdrive" >> "$T/define.out" 2>/dev/null)
sleep 6
grep 'this pc drive' "$T/fe.log" | tail -n +"$((before + 1))" | grep -q 'L"R:' &&
    pass "This PC lists a drive that came, without being opened again" || fail "This PC did not list R: ($(grep -c 'this pc drive' "$T/fe.log") lines)"
mid=$(grep -c 'this pc drive' "$T/fe.log")
(cd "$WINEPREFIX/drive_c" && "$WINE" drivewatch-probe.exe remove R >> "$T/define.out" 2>/dev/null)
sleep 6
after=$(grep 'this pc drive' "$T/fe.log" | tail -n +"$((mid + 1))")
[ -n "$after" ] && ! printf '%s\n' "$after" | grep -q 'L"R:' &&
    pass "and leaves it out once it is gone" || fail "after removal: $(printf '%s' "$after" | tr '\n' '|')"
sleep 2
grep -q '^arrival mask=20000' "$T/probe.out" && pass "a program hears WM_DEVICECHANGE: the drive arrived (R:)" ||
    fail "no arrival: $(tr '\n' '|' < "$T/probe.out")"
grep -q '^removal mask=20000' "$T/probe.out" && pass "and that it was removed" ||
    fail "no removal: $(tr '\n' '|' < "$T/probe.out")"
grep -q "define R ok" "$T/define.out" && grep -q "remove R ok" "$T/define.out" || echo "      mount manager: $(tr '\n' '|' < "$T/define.out" 2>/dev/null)"
exit $RC
