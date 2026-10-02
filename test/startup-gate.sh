#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The programs that start at sign-in (patches/sg/0754): the shell runs the Run
# keys (the machine's in both registry views, the person's), the person's
# RunOnce (removed as it runs) and the Startup folders -- once per sign-in,
# not those turned off in Task Manager (StartupApproved). Nothing ran them in
# a session: AmbirScan's, athenaNet Device Manager's and DYMO's tray programs
# never started (David 2026-10-01).
#
# Each entry appends a line to its own file. Explorer started for sign-in 11:
# each enabled entry once, the turned-off one never, RunOnce gone. Explorer
# started again in sign-in 11: nothing more. Sign-in 12: the enabled ones again,
# RunOnce not.
#
#   WINE=/opt/wine-sg/bin/wine test/startup-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-startup.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
M='C:\m'
mkdir -p "$WINEPREFIX/drive_c/m"
R='Software\Microsoft\Windows\CurrentVersion'
add() { "$WINE" reg add "$1" /v "$2" /d "cmd /c echo x>>$M\\$2.txt" /f ${3:-} >/dev/null 2>&1; }
add "HKLM\\$R\\Run" hklm64 /reg:64
add "HKLM\\$R\\Run" hklm32 /reg:32
add "HKCU\\$R\\Run" hkcu
add "HKCU\\$R\\Run" off
add "HKCU\\$R\\RunOnce" once
"$WINE" reg add "HKCU\\$R\\Explorer\\StartupApproved\\Run" /v off /t REG_BINARY /d 030000000000000000000000 /f >/dev/null 2>&1
S="$WINEPREFIX/drive_c/ProgramData/Microsoft/Windows/Start Menu/Programs/StartUp"
mkdir -p "$S"
printf 'echo x>>C:\\m\\folder.txt\r\n' > "$S/folder.bat"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 800x600 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF2
#!/bin/sh
cd "$WINEPREFIX/drive_c"
shell() {
    XDG_SESSION_ID=\$1 "$WINE" explorer /desktop=shell,800x600 > "$T/explorer-\$2.out" 2>&1 &
    sleep 12
    pkill -f 'explorer.exe /desktop=shell' ; sleep 2
    for f in hklm64 hklm32 hkcu off once folder; do printf '%s=%s ' \$f \$(cat "$WINEPREFIX/drive_c/m/\$f.txt" 2>/dev/null | wc -l); done > "$T/count-\$2"
}
shell 11 a
shell 11 b
shell 12 c
"$WINE" reg query 'HKCU\\$R\\RunOnce' /v once > "$T/runonce" 2>&1
EOF2
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s "-screen 0 800x600x24" "$T/session.sh" > "$T/session.out" 2>&1
a=$(cat "$T/count-a" 2>/dev/null); b=$(cat "$T/count-b" 2>/dev/null); c=$(cat "$T/count-c" 2>/dev/null)
echo "      sign-in 11: $a"; echo "      again in 11: $b"; echo "      sign-in 12: $c"
[ "$a" = "hklm64=1 hklm32=1 hkcu=1 off=0 once=1 folder=1 " ] \
    && pass "at sign-in: the machine's Run keys (both views), the person's Run and RunOnce, the Startup folder" \
    || fail "at sign-in: $a"
grep -q 'off=0' "$T/count-a" && pass "not an entry turned off in Task Manager (StartupApproved)" || fail "the turned-off entry ran: $a"
grep -qi 'unable to find\|not find' "$T/runonce" && pass "RunOnce's value is gone once it ran" || fail "RunOnce kept: $(tr -d '\r' < "$T/runonce" | head -3)"
[ "$b" = "$a" ] && pass "the shell started again in the same sign-in starts nothing again" || fail "explorer restarted: $b (was $a)"
[ "$c" = "hklm64=2 hklm32=2 hkcu=2 off=0 once=1 folder=2 " ] && pass "the next sign-in: all again, RunOnce not" || fail "next sign-in: $c"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
