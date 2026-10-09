#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Shell and user odds and ends (patches/sg/1686), on Xvfb:
# test/shellmisc-probe.c hides a window's caption text, icon and system
# menu with SetWindowThemeAttribute(WTA_NONCLIENT) (Wine draws the frames:
# Decorated=N), waits for RegisterGPNotification's event on a policy change
# and on RefreshPolicy, holds EnterCriticalPolicySection against another
# thread, asks SHQueryUserNotificationState with a full-screen window in
# front and with notifications off, and calls CancelDC. These were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/guires-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_NO_WTA (win32u/defwnd.c), SG_MUTANT_NO_GP_NOTIFY
# (userenv/userenv_main.c), SG_MUTANT_ALWAYS_ACCEPTS (shell32/shell32_main.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-shellmisc.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/shellmisc-probe.exe" "$HERE/shellmisc-probe.c" -luser32 -lgdi32 -ladvapi32 \
    || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\X11 Driver' /v Decorated /d N /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/shellmisc-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" shellmisc-probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
