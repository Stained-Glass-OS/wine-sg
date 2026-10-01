#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Print Screen (patches/sg/0640): screen snipping while no program holds the
# key; a screenshot tool that registers it as its hotkey (Greenshot, ShareX)
# gets it. The shell held it as a hotkey of its own and Greenshot said "the
# hotkey(s) PrintScreen could not be registered" (David 2026-10-01).
#
#   WINE=/opt/wine-sg/bin/wine test/prtsc-gate.sh
# Mutation: build programs/explorer with -DSG_MUTANT_PRTSC_HELD (the shell
# registers the key again): the tool's registration fails.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-prtsc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/prtsc-probe.exe" "$HERE/prtsc-probe.c" &&
"$MINGW" -municode -O2 -o "$T/control-probe.exe" "$HERE/control-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/prtsc-probe.exe" "$T/control-probe.exe" "$WINEPREFIX/drive_c/"
# screen snipping: a stand-in that records how it was started
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\snippingtool.exe' /ve /d 'C:\control-probe.exe' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
xdotool key Print; sleep 3
cp standin.log "$T/after-free.log" 2>/dev/null; rm -f standin.log
"$WINE" prtsc-probe.exe > "$T/probe.out" 2>/dev/null &
sleep 3
xdotool key Print; sleep 3
wait
EOS
chmod +x "$T/session.sh"
timeout -s KILL 180 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"

grep -q '/clip' "$T/after-free.log" 2>/dev/null && pass "Print Screen opens screen snipping while no program holds it" \
    || fail "free Print Screen: $(cat "$T/after-free.log" 2>/dev/null)"
grep -q '^registered=1' "$T/probe.out" 2>/dev/null && pass "a screenshot tool can make Print Screen its hotkey with the shell running" \
    || fail "the tool's RegisterHotKey: $(cat "$T/probe.out" 2>/dev/null)"
grep -q '^hotkey' "$WINEPREFIX/drive_c/prtsc.log" 2>/dev/null && pass "and then the key reaches the tool" || fail "the tool got no WM_HOTKEY"
[ ! -s "$WINEPREFIX/drive_c/standin.log" ] && pass "and not screen snipping" || fail "snipping opened too: $(cat "$WINEPREFIX/drive_c/standin.log")"
exit $RC
