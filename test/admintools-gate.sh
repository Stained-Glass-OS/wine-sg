#!/bin/sh
# The Windows administrative tools' names (patches/sg/0142).
#
# mmc.exe, eventvwr.exe, resmon.exe and cleanmgr.exe are system32 launchers for
# what App Paths registers (sg-shell's consoles), as calc.exe is (0120);
# Wine's msinfo32.exe hands off the same way. wineboot writes services.msc,
# eventvwr.msc, devmgmt.msc, diskmgmt.msc and compmgmt.msc to system32, and
# .msc opens with mmc.exe. The Start menu gets Windows Administrative Tools
# shortcuts, and Win+X lists Event Viewer, Device Manager, Disk Management and
# Computer Management.
#
#   WINE=/opt/wine-sg/bin/wine test/admintools-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v "$MINGW32" >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-admintools.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/handoff-probe.c" &&
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/handoff-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/probe64.exe"; cp "$T/probe32.exe" "$C/probe32.exe"; cp "$T/probe64.exe" "$C/standin.exe"

for n in mmc eventvwr resmon cleanmgr msinfo32; do
    for d in system32 syswow64; do
        [ -f "$C/windows/$d/$n.exe" ] && pass "$d\\$n.exe exists" || fail "no $d\\$n.exe"
    done
done
for m in services eventvwr devmgmt diskmgmt compmgmt; do
    f="$C/windows/system32/$m.msc"
    grep -q "<StainedGlass Console=\"$m\"" "$f" 2>/dev/null && pass "system32\\$m.msc names its console" \
        || fail "system32\\$m.msc missing or wrong"
done
ADM="$C/ProgramData/Microsoft/Windows/Start Menu/Programs/Administrative Tools"
for l in "Computer Management" "Device Manager" "Disk Cleanup" "Disk Management" "Event Viewer" \
         "Registry Editor" "Resource Monitor" "Services" "System Information"; do
    [ -f "$ADM/$l.lnk" ] && pass "Start menu: Administrative Tools\\$l" || fail "no Administrative Tools\\$l.lnk"
done
assoc=$("$WINE" reg query 'HKCR\MSCFile\shell\open\command' /ve 2>/dev/null | tr -d '\r' | grep REG_)
case "$assoc" in *mmc.exe*%1*) pass ".msc opens with mmc.exe ($assoc)" ;; *) fail ".msc association: '$assoc'" ;; esac

AP='HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths'
for n in mmc eventvwr resmon cleanmgr msinfo32; do
    "$WINE" reg add "$AP\\$n.exe" /ve /d 'C:\standin.exe' /f >/dev/null 2>&1
done
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$C"
P() { echo "\$*" >> "$T/log.out"; "$WINE" "\$@" 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
WINEDEBUG=err+all "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
n=0
mark() { echo "== \$1" >> "$C/standin.log"; }
seen() { n=\$((n + 1)); i=0; while [ "\$(grep -c '^cmdline=' "$C/standin.log" 2>/dev/null)" -lt \$n ] && [ \$i -lt 20 ]; do sleep 0.5; i=\$((i + 1)); done; }
mark mmc64;      P probe64.exe run 'mmc.exe C:\\windows\\system32\\services.msc /s'; seen
mark eventvwr64; P probe64.exe run 'eventvwr.exe /l:Application'; seen
mark resmon64;   P probe64.exe run 'resmon.exe'; seen
mark cleanmgr64; P probe64.exe run 'cleanmgr /d c'; seen
mark msinfo64;   P probe64.exe run 'msinfo32.exe /report "C:\\my report.txt"'; seen
mark eventvwr32; P probe32.exe run 'eventvwr'; seen
mark msinfo32;   P probe32.exe run 'msinfo32'; seen
mark services;   P probe64.exe open services.msc; seen
mark devmgmt;    P probe64.exe open devmgmt.msc; seen
mark lnk;        P probe64.exe open 'C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs\\Administrative Tools\\Services.lnk'; seen
"$WINE" notepad >/dev/null 2>&1 &
sleep 3; xdotool mousemove 200 150 click 1; sleep 1
mark winx_v
xdotool key super+x; sleep 1.5; xdotool key v; seen
mark winx_m
xdotool key super+x; sleep 1.5; xdotool key m; seen
mark winx_i
xdotool key super+x; sleep 1.5; xdotool key i; seen
mark winx_g
xdotool key super+x; sleep 1.5; xdotool key g; seen
mark end
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
tr -d '\r' < "$C/standin.log" > "$T/standin.txt" 2>/dev/null
sed 's/^/      /' "$T/log.out"; sed 's/^/      | /' "$T/standin.txt"

got() { awk -v m="== $1" '$0 == m { on = 1; next } /^== / { on = 0 } on' "$T/standin.txt"; }
want() {
    if [ "$(got "$1")" = "cmdline=$2" ]; then pass "$3"; else fail "$3: got '$(got "$1")'"; fi
}
wantlike() {  # $1 mark, $2 glob, $3 what
    case "$(got "$1")" in cmdline=$2) pass "$3" ;; *) fail "$3: got '$(got "$1")'" ;; esac
}
want mmc64 '"C:\standin.exe" C:\windows\system32\services.msc /s' 'mmc.exe hands a console file to the App Paths console host'
want eventvwr64 '"C:\standin.exe" /l:Application' 'eventvwr.exe hands off with its arguments'
want resmon64 '"C:\standin.exe"' 'resmon.exe hands off'
want cleanmgr64 '"C:\standin.exe" /d c' 'cleanmgr (no .exe) hands off'
want msinfo64 '"C:\standin.exe" /report "C:\my report.txt"' "Wine's msinfo32.exe hands off to the App Paths System Information"
want eventvwr32 '"C:\standin.exe"' 'eventvwr from a 32-bit program (syswow64)'
want msinfo32 '"C:\standin.exe"' 'msinfo32 from a 32-bit program'
wantlike services '*"C:\\windows\\system32\\services.msc"*' 'ShellExecute of services.msc (the Run box) opens it through mmc.exe'
wantlike devmgmt '*"C:\\windows\\system32\\devmgmt.msc"*' 'ShellExecute of devmgmt.msc'
wantlike lnk '*services.msc*' "the Start menu's Services shortcut opens services.msc"
want winx_v '"C:\standin.exe"' 'Win+X, V: Event Viewer'
wantlike winx_m '*devmgmt.msc*' 'Win+X, M: Device Manager'
wantlike winx_i '*diskmgmt.msc*' 'Win+X, I: Disk Management'
wantlike winx_g '*compmgmt.msc*' 'Win+X, G: Computer Management'
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
