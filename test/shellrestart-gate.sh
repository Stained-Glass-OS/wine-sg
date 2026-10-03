#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell comes back after it crashes while programs run (patches/sg/0755).
#
# A program on the desktop keeps the Wine desktop alive when the shell dies
# (a crashed one kept by its debugger, a tray program); the desktop's X window
# died with the shell. The shell started again took that dead window as the
# desktop, its first window failed (BadWindow) and so did every restart: the
# session never got past "Getting things ready" (David 2026-10-01, after
# AmbirScan crashed the desktop). Now it shows the desktop in its own window.
#
# holddesk-probe stays on the desktop with no window on the screen; explorer is killed (SIGKILL, as a crash) and
# started again: no X error, and a desktop X window with the shell's windows
# (its taskbar) in it.
#
# The old desktop window lives on detached (no thread); wineserver made the
# new shell's desktop window a child of it, its WM_NCCREATE refused that and
# the shell exited -- now the new shell takes the detached window over
# (patches/sg/0761). Mutants: SG_MUTANT_STALE_DESKTOP (BadWindow),
# SG_MUTANT_NO_DESKTOP_TAKEOVER (no desktop after the restart).
#
# Restarted again with the spies still connected, the new shell gets the dead
# one's client ids and its desktop window the dead one's id: it was taken for
# alive and never shown (patches/sg/0770). Mutant: SG_MUTANT_REUSED_ID_ALIVE.
# SG_MUTANT_RESTART_UNMAPPED (the new window not mapped) passes here: Xvfb,
# with no window manager, shows it anyway; the compositor's did not (QA VM).
#
#   WINE=/opt/wine-sg/bin/wine test/shellrestart-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
for t in xvfb-run xwininfo xprop; do command -v $t >/dev/null || { echo "SKIP: needs $t"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-shellrestart.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/holddesk-probe.exe" "$HERE/holddesk-probe.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/holddesk-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
desk() { xwininfo -root -tree | awk '/"shell - Wine Desktop"/ { print \$1; exit }'; }
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer1.out" 2>&1 &
sleep 6
"$WINE" holddesk-probe.exe 90 > /dev/null 2>&1 &
sleep 3
desk > "$T/desk1"
pkill -KILL -x explorer.exe
sleep 3
# another X client takes the dead shell's client ids (as in a session, where
# others connect between): the restarted shell's windows get other ids
H=""
for i in 1 2 3 4 5 6; do xprop -spy -root > /dev/null 2>&1 & H="\$H \$!"; done
sleep 1
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer2.out" 2>&1 &
i=0; while [ \$i -lt 40 ] && [ -z "\$(desk)" ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
d=\$(desk); echo "\$d" > "$T/desk2"
[ -n "\$d" ] && xwininfo -id "\$d" -children | grep -c '^ *0x' > "$T/children2"
[ -n "\$d" ] && xwininfo -id "\$d" | sed -n 's/.*Map State: //p' > "$T/mapped2"
# again, with no other client between (the spies stay: the dead shell's
# client slots are the lowest free ones): the X server gives the new shell
# the dead one's ids, its desktop window the same id as the dead one's
# (Xwayland did, in a session: patches/sg/0770)
sleep 2
pkill -KILL -x explorer.exe
sleep 3
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer3.out" 2>&1 &
i=0; while [ \$i -lt 40 ] && [ -z "\$(desk)" ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
d3=\$(desk); echo "\$d3" > "$T/desk3"
[ -n "\$d3" ] && xwininfo -id "\$d3" | sed -n 's/.*Map State: //p' > "$T/mapped3"
kill \$H 2>/dev/null
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 240 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > "$T/session.out" 2>&1
echo "      desktop before: $(cat "$T/desk1" 2>/dev/null), after: $(cat "$T/desk2" 2>/dev/null) ($(cat "$T/children2" 2>/dev/null) windows)"
grep -q 'BadWindow\|X Error' "$T/explorer2.out" && fail "the shell started again died on the dead window: $(grep -m1 'BadWindow' "$T/explorer2.out")" \
    || pass "the shell started again takes no dead window (no X error)"
[ -n "$(cat "$T/desk2" 2>/dev/null)" ] && [ "$(cat "$T/children2" 2>/dev/null || echo 0)" -ge 1 ] \
    && pass "the desktop shows again, with the shell's windows (the taskbar) in it ($(cat "$T/children2") windows)" \
    || fail "no desktop after the restart: the detached desktop window was not taken over"
[ "$(cat "$T/mapped2" 2>/dev/null)" = IsViewable ] && pass "and it is on the screen (mapped: the new shell showed it)" \
    || fail "the desktop after the restart is not on the screen: $(cat "$T/mapped2" 2>/dev/null) (only the backdrop showed)"
if [ "$(cat "$T/desk3" 2>/dev/null)" = "$(cat "$T/desk2" 2>/dev/null)" ]; then
    [ "$(cat "$T/mapped3" 2>/dev/null)" = IsViewable ] \
        && pass "restarted again with the dead shell's ids (its desktop window the same id): on the screen" \
        || fail "restarted with the dead shell's window id $(cat "$T/desk3"): $(cat "$T/mapped3" 2>/dev/null) -- the desktop never shown"
else
    echo "      (the second restart's desktop got another id, $(cat "$T/desk3" 2>/dev/null) after $(cat "$T/desk2" 2>/dev/null): the reused-id case not reached)"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
