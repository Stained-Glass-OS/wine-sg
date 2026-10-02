#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Wine's windows and the Linux programs' windows take turns in front
# (patches/sg/0700). Every Wine window lives in Wine's desktop window, one X
# window under a Linux program's (SG Office's editors). While the
# compositor's list (sg-lockctl XWINDOWS) says a Linux window has the focus:
#   * a Wine window coming forward (a program starting) asks the compositor
#     for the desktop in front (XDESKTOP), once;
#   * Wine's last active window is deactivated, so that coming forward again
#     is noticed;
#   * its taskbar button brings it forward (XDESKTOP), not minimized for
#     being Wine's active window;
#   * a Linux window brought forward by ITS button deactivates Wine's active
#     window too, so a click on that Wine window brings it forward (0747;
#     mutant SG_MUTANT_XWIN_BUTTON_KEEPS_ACTIVE fails it).
# A stand-in sg-lockctl (SG_LOCKCTL) serves the list the gate writes and
# records what it is asked.
#
#   WINE=/opt/wine-sg/bin/wine test/desktopfront-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v cc >/dev/null || { echo "SKIP: needs $MINGW and cc"; exit 77; }
command -v xvfb-run >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-dfront.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { fail "stand-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
# a Wine program's window, and what Wine says of it: "fg=<foreground title> iconic=<0|1>"
cat > "$T/fgwin.c" <<'EOF'
#include <windows.h>
#include <stdio.h>
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }
static HWND make_popup(void)
{
    WNDCLASSW wc = { 0 }; HWND h;
    wc.lpfnWndProc = proc; wc.lpszClassName = L"SgFrontPopup"; wc.hbrBackground = GetStockObject(GRAY_BRUSH);
    RegisterClassW(&wc);
    h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"SgFrontPopup", L"Start-like", WS_POPUP, 0, 300, 300, 360, 0, 0, 0, 0);
    ShowWindow(h, SW_SHOW); SetForegroundWindow(h);
    return h;
}
static void pump(DWORD ms)
{
    MSG msg; DWORD end = GetTickCount() + ms;
    while ((int)(end - GetTickCount()) > 0)
    {
        MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
}
int wmain(int argc, WCHAR **argv)
{
    if (argc > 1 && (!lstrcmpW(argv[1], L"popup") || !lstrcmpW(argv[1], L"popuplaunch")))
    {
        /* a Start menu: up, then gone (Esc) -- or gone, and a program's window after */
        HWND p = make_popup();
        pump(3000);
        ShowWindow(p, SW_HIDE);
        if (!lstrcmpW(argv[1], L"popuplaunch"))
        {
            WNDCLASSW wc = { 0 }; HWND h;
            pump(600);
            wc.lpfnWndProc = proc; wc.lpszClassName = L"SgFrontLaunched"; wc.hbrBackground = GetStockObject(WHITE_BRUSH);
            RegisterClassW(&wc);
            h = CreateWindowW(L"SgFrontLaunched", L"Launched", WS_OVERLAPPEDWINDOW, 200, 150, 400, 300, 0, 0, 0, 0);
            ShowWindow(h, SW_SHOWNORMAL); SetForegroundWindow(h);
        }
        pump(6000);
        return 0;
    }
    if (argc > 1 && !lstrcmpW(argv[1], L"show"))
    {
        WNDCLASSW wc = { 0 }; MSG msg; HWND h;
        wc.lpfnWndProc = proc; wc.lpszClassName = L"SgFrontProbe"; wc.hbrBackground = GetStockObject(WHITE_BRUSH);
        RegisterClassW(&wc);
        h = CreateWindowW(L"SgFrontProbe", L"Front Probe", WS_OVERLAPPEDWINDOW, 100, 100, 400, 300, 0, 0, 0, 0);
        ShowWindow(h, SW_SHOWNORMAL); SetForegroundWindow(h);
        while (GetMessageW(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        return 0;
    }
    else
    {
        WCHAR t[128] = L"(none)"; HWND fg = GetForegroundWindow(), p = FindWindowW(L"SgFrontProbe", NULL);
        if (fg) GetWindowTextW(fg, t, 128);
        printf("fg=%ls iconic=%d\n", t, p ? IsIconic(p) : -1);
    }
    return 0;
}
EOF
"$MINGW" -municode -O2 -o "$T/fgwin.exe" "$T/fgwin.c" -luser32 -lgdi32 || { fail "fgwin did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$T/fgwin.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
TAB=$(printf '\t')
FOCUSED="4242 shown focused SG Office${TAB}Document1 - SG Office Documents"
AWAY="4242 shown - SG Office${TAB}Document1 - SG Office Documents"
printf '%s\nEND\n' "$FOCUSED" > "$T/list"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run" SG_FAKE_FOCUS=1
mid() { echo "\$1" | awk -F, '{ printf "%d %d", (\$1 + \$3) / 2, (\$2 + \$4) / 2 }'; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 5
# a Wine program starts while SG Office has the focus
"$WINE" fgwin.exe show & sleep 4
cp "$T/commands" "$T/commands.started" 2>/dev/null
# the compositor: the desktop has the focus now; then the person clicks SG Office
printf '%s\nEND\n' "$AWAY" > "$T/list"; sleep 3
printf '%s\nEND\n' "$FOCUSED" > "$T/list"; sleep 5
"$WINE" fgwin.exe 2>/dev/null | tr -d '\r' > "$T/fg.deactivated"
# its button: the probe's window is the bar's last (SG Office's came first)
B=\$("$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | sed -n 's/^button=//p' | tail -1)
echo "button \$B" > "$T/button"
xdotool mousemove \$(mid "\$B") click 1; sleep 3
"$WINE" fgwin.exe 2>/dev/null | tr -d '\r' > "$T/fg.clicked"
cp "$T/commands" "$T/commands.button" 2>/dev/null
# SG Office has the focus again (a click on it); a Start-like popup comes and goes (Esc)
printf '%s\nEND\n' "$FOCUSED" > "$T/list"; sleep 5
: > "$T/commands"
"$WINE" fgwin.exe popup; sleep 3
cp "$T/commands" "$T/commands.popup"
# again, and this time it starts a program as it goes
printf '%s\nEND\n' "$FOCUSED" > "$T/list"; sleep 5
: > "$T/commands"
"$WINE" fgwin.exe popuplaunch; sleep 3
cp "$T/commands" "$T/commands.launch"
# SG Office brought forward by ITS taskbar button (the bar's first) while a
# Wine window ("Launched") is Wine's active one; then a click on that Wine
# window must bring it -- and the desktop -- forward again (0747)
printf '%s\nEND\n' "$AWAY" > "$T/list"; sleep 4
xdotool mousemove 400 300 click 1; sleep 2
"$WINE" fgwin.exe 2>/dev/null | tr -d '\r' > "$T/fg.before"
: > "$T/commands"
X=\$("$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' | sed -n 's/^button=//p' | head -1)
xdotool mousemove \$(mid "\$X") click 1; sleep 1
printf '%s\nEND\n' "$FOCUSED" > "$T/list"; sleep 4
"$WINE" fgwin.exe 2>/dev/null | tr -d '\r' > "$T/fg.xbutton"
: > "$T/commands"
xdotool mousemove 400 300 click 1; sleep 3
cp "$T/commands" "$T/commands.reclick"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
echo "      commands: $(tr '\n' '|' < "$T/commands.button" 2>/dev/null)"
echo "      after SG Office got the focus: $(cat "$T/fg.deactivated" 2>/dev/null); $(cat "$T/button" 2>/dev/null)"
echo "      after its button: $(cat "$T/fg.clicked" 2>/dev/null)"

[ "$(grep -c '^XDESKTOP$' "$T/commands.started" 2>/dev/null)" = 1 ] \
    && pass "a Wine program's window coming forward puts the desktop in front (XDESKTOP), once" \
    || fail "on start: $(tr '\n' '|' < "$T/commands.started" 2>/dev/null)"
grep -q "fg=Front Probe" "$T/fg.deactivated" 2>/dev/null \
    && fail "SG Office has the focus, yet the Wine window is still Wine's active one" \
    || pass "SG Office has the focus: Wine's last active window is deactivated ($(cat "$T/fg.deactivated" 2>/dev/null))"
[ "$(grep -c '^XDESKTOP$' "$T/commands.button" 2>/dev/null)" = 2 ] \
    && pass "its taskbar button puts the desktop in front again (XDESKTOP)" \
    || fail "button: $(tr '\n' '|' < "$T/commands.button" 2>/dev/null)"
grep -q "fg=Front Probe iconic=0" "$T/fg.clicked" 2>/dev/null \
    && pass "and brings the window forward, not minimized" || fail "after the button: $(cat "$T/fg.clicked" 2>/dev/null)"

echo "      popup: $(tr '\n' '|' < "$T/commands.popup" 2>/dev/null)"
echo "      popup + program: $(tr '\n' '|' < "$T/commands.launch" 2>/dev/null)"
[ "$(tr '\n' '|' < "$T/commands.popup" 2>/dev/null)" = "XDESKTOP|XACTIVATE 4242|" ] \
    && pass "a Start-like popup puts the desktop in front; gone without starting anything, SG Office comes back" \
    || fail "popup: $(tr '\n' '|' < "$T/commands.popup" 2>/dev/null)"
[ "$(tr '\n' '|' < "$T/commands.launch" 2>/dev/null)" = "XDESKTOP|" ] \
    && pass "a popup that starts a program leaves the desktop (and the program) in front" \
    || fail "popup + program: $(tr '\n' '|' < "$T/commands.launch" 2>/dev/null)"

echo "      before SG Office's own button: $(cat "$T/fg.before" 2>/dev/null)"
echo "      after SG Office's own button: $(cat "$T/fg.xbutton" 2>/dev/null)"
BEFORE=$(sed -n 's/^fg=\([^ ]*\).*/\1/p' "$T/fg.before" 2>/dev/null)
[ -n "$BEFORE" ] && [ "$BEFORE" != "(none)" ] || fail "no Wine window was Wine's active one to begin with: $(cat "$T/fg.before" 2>/dev/null)"
echo "      a click on the Wine window then: $(tr '\n' '|' < "$T/commands.reclick" 2>/dev/null)"
[ -n "$BEFORE" ] && grep -q "fg=$BEFORE " "$T/fg.xbutton" 2>/dev/null \
    && fail "SG Office came forward by its button, yet the Wine window is still Wine's active one" \
    || pass "SG Office brought forward by its button: Wine's active window is deactivated ($(cat "$T/fg.xbutton" 2>/dev/null))"
grep -q '^XDESKTOP$' "$T/commands.reclick" 2>/dev/null \
    && pass "and a click on the Wine window behind it puts the desktop in front (XDESKTOP)" \
    || fail "click on the Wine window: $(tr '\n' '|' < "$T/commands.reclick" 2>/dev/null)"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
