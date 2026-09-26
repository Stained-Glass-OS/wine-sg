#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The Run dialog (Win+R, shell32's RunFileDlg; patch 0266), in a shell session.
#
# Win+R typed on the X keyboard opens it; what is typed next goes into its box
# at once, with no click first (it opened behind, without the focus, when the
# shell -- a background process -- showed it); Enter runs it. Its text says
# Stained Glass, not Wine, and its picture is the shell's Run icon, not Wine's
# glass (the icon's pixels are checked against the glass's red wine).
#
#   WINE=/opt/wine-sg/bin/wine test/rundialog-gate.sh
#   ARTIFACTS=DIR keeps the screenshots and logs
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null && command -v xdotool >/dev/null || { echo "SKIP: needs xvfb-run, ImageMagick and xdotool"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
case "${DISPLAY:-}" in :0|:0.*) unset DISPLAY ;; esac   # never a person's desktop

T=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-rundlg.XXXXXX")
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.out "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

# the probe: the Run dialog's description text and icon rectangle, whether it
# is the foreground window, and what its box holds
cat > "$T/probe.c" <<'EOF'
#include <windows.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc > 1)   /* wait for a top-level window of this class (up to 120 s) */
    {
        int i;
        for (i = 0; i < 1200 && !FindWindowA(argv[1], NULL); i++) Sleep(100);
        printf("waited=%d\n", FindWindowA(argv[1], NULL) != NULL);
        return 0;
    }
    HWND dlg = FindWindowW(L"#32770", L"Run"), fg = GetForegroundWindow(), box, desc, icon;
    WCHAR text[512] = L"", edit[256] = L"";
    RECT r = {0};
    if (!dlg) { printf("dialog=none\n"); return 0; }
    desc = GetDlgItem(dlg, 12289); box = GetDlgItem(dlg, 12298); icon = GetDlgItem(dlg, 12297);
    GetWindowTextW(desc, text, 512);
    if (box) SendMessageW(box, WM_GETTEXT, 256, (LPARAM)edit);   /* another process: not GetWindowText */
    if (icon) GetWindowRect(icon, &r);
    {
        /* another process's focus: its GUI thread's */
        GUITHREADINFO gui = { sizeof(gui) };
        WCHAR cls[64] = L"";
        DWORD tid = GetWindowThreadProcessId(dlg, NULL);
        GetGUIThreadInfo(tid, &gui);
        if (gui.hwndFocus) GetClassNameW(gui.hwndFocus, cls, 64);
        printf("dialog=up foreground=%d focusbox=%d\n", fg == dlg,
               gui.hwndFocus && (gui.hwndFocus == box || GetParent(gui.hwndFocus) == box));
        printf("focusclass=%ls active=%d\n", cls, gui.hwndActive == dlg);
    }
    printf("text=%ls\nbox=%ls\nicon=%ld,%ld,%ld,%ld\n", text, edit, r.left, r.top, r.right, r.bottom);
    return 0;
}
EOF
"$MINGW" -O2 -o "$T/probe.exe" "$T/probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
P() { echo "== \$1" >> "$T/log.out"; "$WINE" probe.exe 2>/dev/null | tr -d '\r' >> "$T/log.out"; }
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
# something else in front first, as when a person presses Win+R while working
"$WINE" notepad >/dev/null 2>&1 &
"$WINE" probe.exe Notepad >> "$T/log.out" 2>/dev/null
sleep 3
xdotool key super+r; sleep 2
P opened; import -window root "$T/run.png"
# no click: the keys go straight in
xdotool type --delay 60 winver; sleep 1
P typed
xdotool key Return; sleep 3
P after
ps -eo args | grep -i '[w]inver' > "$T/ps.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
sed 's/^/      /' "$T/log.out"

v() { awk -v s="== $1" -v k="$2" '$0 == s { on = 1; next } /^== / { on = 0 } on && index($0, k "=") == 1 { sub(k "=", ""); print; exit }' "$T/log.out"; }

case "$(v opened dialog)" in up*) pass "Win+R opens the Run dialog" ;; *) fail "no Run dialog after Win+R: $(v opened dialog)" ;; esac
case "$(v opened dialog)" in *"foreground=1 focusbox=1"*) pass "it opens in front, with the focus in its box" ;; *) fail "not in front / no focus: $(v opened dialog)" ;; esac
[ "$(v typed box)" = winver ] && pass "typing goes straight into the box, no click first" || fail "the box holds '$(v typed box)'"
[ -s "$T/ps.out" ] && pass "Enter runs it" || fail "winver did not start"
case "$(v opened text)" in *"Stained Glass will open it for you"*) pass "the text says Stained Glass" ;; *) fail "text: $(v opened text)" ;; esac
case "$(v opened text)" in *Wine*) fail "the text still names Wine" ;; *) pass "and not Wine" ;; esac
# Wine's glass: a red wine fill. The Run icon has none.
set -- $(v opened icon | tr ',' ' ')
if [ $# -eq 4 ] && [ "$3" -gt "$1" ]; then
    red=$(convert "$T/run.png" -crop "$(($3 - $1))x$(($4 - $2))+$1+$2" +repage -depth 8 txt:- 2>/dev/null \
        | awk -F'[(,)]' 'NR > 1 && $3 > 115 && $4 < 51 && $5 < 64 { n++ } END { print n + 0 }')
    [ "${red:-99}" -lt 3 ] && pass "its picture is the shell's Run icon, not Wine's glass ($red red pixels)" || fail "Wine's glass is still there ($red red pixels)"
else fail "no icon rectangle: $(v opened icon)"; fi

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
