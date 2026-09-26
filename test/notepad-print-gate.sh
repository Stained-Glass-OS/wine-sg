#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Notepad's dark menu bar and its Page Setup header and footer (patches/sg/0100
# and 0188), in a shell session under Xvfb.
#
#  - with Notepad's own theme dark (View > Theme, sgTheme=2) the menu bar is
#    dark and its items light (Notepad draws them; win32u paints the bar with
#    its MIM_BACKGROUND brush -- 0188), Alt+F still opens File; with the light
#    theme the bar is light again;
#  - File > Page Setup (a printer from a private, unprivileged cupsd the gate
#    runs: Wine's dialogs need one) has Header and Footer boxes; what is typed
#    there is kept (szHeader / szTrailer, as Windows keeps them);
#  - printing (/p, NOTEPAD_PRINT_EMF=<dir> writes each page as an EMF) puts
#    the header's &l part on the left, &r on the right, &c (and uncoded text)
#    centred, with &f/&p/&d expanded, on every page, and the footer likewise.
#
#   WINE=/opt/wine-sg/bin/wine test/notepad-print-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
#   ARTIFACTS=DIR keeps screenshots
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${NOTEPAD_PRINT_DPY:-198}"
RC=0; XP=""; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb xdotool xclip import python3 "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x /usr/sbin/cupsd ] && [ -x /usr/sbin/lpadmin ] || { echo "SKIP: needs cups-daemon and cups-client"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3-pil missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-notepad-print.XXXXXX); chmod 755 "$T"
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    [ -n "$CP" ] && kill "$CP" 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$ARTIFACTS"/ 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

# a private CUPS with one raw printer writing to a file: a default printer
# for Wine's Page Setup and Print dialogs, and nothing of this machine's
mkdir -p "$T/cups/spool" "$T/cups/cache" "$T/cups/state" "$T/cups/logs" "$T/cups/tmp"
cat > "$T/cups/cupsd.conf" <<EOF
Listen $T/cups/cups.sock
LogLevel warn
DefaultAuthType None
WebInterface No
<Location />
  Order allow,deny
  Allow all
</Location>
<Location /admin>
  Order allow,deny
  Allow all
</Location>
EOF
cat > "$T/cups/cups-files.conf" <<EOF
ServerRoot $T/cups
CacheDir $T/cups/cache
StateDir $T/cups/state
RequestRoot $T/cups/spool
TempDir $T/cups/tmp
ServerBin /usr/lib/cups
DataDir /usr/share/cups
AccessLog $T/cups/logs/access_log
ErrorLog $T/cups/logs/error_log
PageLog $T/cups/logs/page_log
FileDevice Yes
Sandboxing relaxed
EOF
/usr/sbin/cupsd -f -c "$T/cups/cupsd.conf" -s "$T/cups/cups-files.conf" >/dev/null 2>&1 & CP=$!
export CUPS_SERVER="$T/cups/cups.sock"
i=0; while [ ! -S "$CUPS_SERVER" ] && [ $i -lt 20 ]; do sleep 0.3; i=$((i + 1)); done
/usr/sbin/lpadmin -p sgtest -E -v "file://$T/cups/out.prn" && /usr/sbin/lpadmin -d sgtest || { fail "no test printer"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cat > "$T/probe.c" <<'EOF'
#include <windows.h>
#include <stdio.h>
/* menubar: the Notepad window's menu bar and its first item, on the screen */
int wmain(int argc, WCHAR **argv)
{
    HWND w = FindWindowW(L"Notepad", NULL);
    MENUBARINFO mbi = { sizeof(mbi) };
    if (!w) { puts("NOWINDOW"); return 1; }
    if (argc > 1 && !wcscmp(argv[1], L"front")) { SetForegroundWindow(w); puts("OK"); return 0; }
    if (argc > 1 && !wcscmp(argv[1], L"close")) { PostMessageW(w, WM_CLOSE, 0, 0); puts("OK"); return 0; }
    {
        /* the bar: the strip above the client area; its first item */
        RECT wr, cr, ir;
        POINT o = { 0, 0 };
        GetWindowRect(w, &wr);
        GetClientRect(w, &cr);
        ClientToScreen(w, &o);
        printf("bar %ld %ld %ld %ld\n", o.x, o.y - GetSystemMetrics(SM_CYMENU), o.x + cr.right, o.y - 1);
        if (!GetMenuItemRect(w, GetMenu(w), 0, &ir))
            SetRect(&ir, o.x, o.y - GetSystemMetrics(SM_CYMENU), o.x + 40, o.y - 1);   /* File: the bar's start */
        printf("item %ld %ld %ld %ld\n", ir.left, ir.top, ir.right, ir.bottom);
    }
    (void)mbi;
    return 0;
}
EOF
"$MINGW" -municode -O2 -o "$C/probe.exe" "$T/probe.c" -luser32 || { fail "probe did not build"; exit 1; }
reg() { "$WINE" reg add "$@" /f >/dev/null 2>&1; }
reg 'HKCU\Software\Wine\Explorer' /v Desktop /d shell
reg 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700
reg 'HKCU\Software\Microsoft\Notepad' /v sgTheme /t REG_DWORD /d 2
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
mkdir -p "$C/t" "$C/pr"
python3 -c "
import sys
open(sys.argv[1], 'w', newline='').write(''.join('printed line %d of the notes\r\n' % i for i in range(1, 121)))" "$C/t/notes.txt"

np_running() { pgrep -x notepad.exe >/dev/null; }
# closed as a person would (it saves its settings on the way out)
np_quit() {
    "$WINE" 'C:\probe.exe' close >/dev/null 2>&1
    i=0; while np_running && [ $i -lt 30 ]; do sleep 0.3; i=$((i + 1)); done
    pkill -x notepad.exe
}
bar() { "$WINE" 'C:\probe.exe' | tr -d '\r' > "$T/bar.out"; }
# the share of dark pixels in a rectangle of the screen: file l t r b
dark_share() {
    python3 - "$@" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("L")
l, t, r, b = map(int, sys.argv[2:6])
px = [im.getpixel((x, y)) for y in range(t, b) for x in range(l, r)]
print(int(100 * sum(1 for p in px if p < 80) / max(1, len(px))), int(100 * sum(1 for p in px if p > 180) / max(1, len(px))))
EOF
}

# --- 1. the dark menu bar --------------------------------------------------------------------------
"$WINE" notepad.exe 'C:\t\notes.txt' >/dev/null 2>&1 &
i=0; while ! "$WINE" 'C:\probe.exe' >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
sleep 2
bar
import -window root "$T/dark.png"
set -- $(sed -n 's/^bar //p' "$T/bar.out")
if [ $# -eq 4 ]; then
    set -- $(dark_share "$T/dark.png" "$1" "$2" "$3" "$4")
    [ "$1" -ge 80 ] && pass "dark theme: the menu bar is dark ($1% dark pixels)" || fail "dark theme: the menu bar is $1% dark"
    set -- $(sed -n 's/^item //p' "$T/bar.out")
    set -- $(dark_share "$T/dark.png" "$1" "$2" "$3" "$4")
    [ "$1" -ge 50 ] && [ "$2" -ge 3 ] && pass "the File item is light text on dark ($1% dark, $2% light pixels)" \
        || fail "File item: $1% dark, $2% light pixels"
else fail "no menu bar: $(cat "$T/bar.out")"; fi
"$WINE" 'C:\probe.exe' front >/dev/null 2>&1; sleep 1
xdotool key alt+f; sleep 1.5
import -window root "$T/dark-menu.png"
set -- $(sed -n 's/^item //p' "$T/bar.out")
# the File menu opened below its item: bright popup pixels under the bar
set -- $(dark_share "$T/dark-menu.png" "$1" "$4" $(( $1 + 120 )) $(( $4 + 60 )))
[ "$2" -ge 50 ] && pass "Alt+F opens the File menu on the owner-drawn bar" || fail "Alt+F: no menu under File ($2% light)"
xdotool key Escape Escape; sleep 0.5

# --- 2. Page Setup: Header and Footer ---------------------------------------------------------------
"$WINE" 'C:\probe.exe' front >/dev/null 2>&1; sleep 1
xdotool key alt+f; sleep 1; xdotool key t; sleep 2.5
import -window root "$T/pagesetup.png"
printf '&lleft &f&cmid&rright &p' | xclip -selection clipboard
xdotool key alt+h; sleep 0.3; xdotool key ctrl+a; xdotool key ctrl+v; sleep 0.3
printf '&cfoot &p of notes' | xclip -selection clipboard
xdotool key alt+f; sleep 0.3; xdotool key ctrl+a; xdotool key ctrl+v; sleep 0.3
import -window root "$T/pagesetup-typed.png"
xdotool key Return; sleep 1
np_quit
H=$("$WINE" reg query 'HKCU\Software\Microsoft\Notepad' /v szHeader 2>/dev/null | tr -d '\r' | sed -n 's/.*REG_SZ *//p')
F=$("$WINE" reg query 'HKCU\Software\Microsoft\Notepad' /v szTrailer 2>/dev/null | tr -d '\r' | sed -n 's/.*REG_SZ *//p')
[ "$H" = '&lleft &f&cmid&rright &p' ] && pass "Page Setup's Header box is kept (szHeader)" || fail "szHeader: '$H'"
[ "$F" = '&cfoot &p of notes' ] && pass "Page Setup's Footer box is kept (szTrailer)" || fail "szTrailer: '$F'"

# --- 3. printed: where the header and footer land ----------------------------------------------------
NOTEPAD_PRINT_EMF='C:\pr' "$WINE" notepad.exe /p 'C:\t\notes.txt' >/dev/null 2>&1
python3 - "$C/pr" > "$T/emf.out" <<'EOF'
import glob, os, struct, sys
# EMR_EXTTEXTOUTW (84): the reference point and the UTF-16 string
pages = sorted(glob.glob(os.path.join(sys.argv[1], "page*.emf")), key=lambda p: int(p.rsplit("page", 1)[1][:-4]))
print("pages", len(pages))
for n, p in enumerate(pages, 1):
    d = open(p, "rb").read()
    off = 0
    while off + 8 <= len(d):
        typ, size = struct.unpack_from("<II", d, off)
        if size < 8: break
        if typ == 84:
            x, y, nch, offs = struct.unpack_from("<iiII", d, off + 36)
            s = d[off + offs: off + offs + 2 * nch].decode("utf-16le", "replace")
            print("text", n, x, y, s)
        off += size
EOF
NP=$(sed -n 's/^pages //p' "$T/emf.out")
[ "${NP:-0}" -ge 2 ] && pass "120 lines print on $NP pages" || fail "printed pages: ${NP:-0}"
line() { awk -v p="$1" -v s="$2" '$1 == "text" && $2 == p { t = $5; for (i = 6; i <= NF; i++) t = t " " $i; if (t == s) { print $3, $4; exit } }' "$T/emf.out"; }
first=$(line 1 "printed line 1 of the notes" | cut -d' ' -f1)
L=$(line 1 "left notes.txt"); M=$(line 1 "mid"); R=$(line 1 "right 1"); F1=$(line 1 "foot 1 of notes"); F2=$(line 2 "foot 2 of notes"); R2=$(line 2 "right 2")
echo "      header left: $L  mid: $M  right: $R  footer p1: $F1  p2: $F2  text x: $first"
[ -n "$L" ] && [ -n "$M" ] && [ -n "$R" ] && pass "the header's parts expand (&f, &p) on page 1" || fail "header parts: left '$L' mid '$M' right '$R'"
if [ -n "$L" ] && [ -n "$M" ] && [ -n "$R" ]; then
    lx=${L% *}; mx=${M% *}; rx=${R% *}; ly=${L#* }; ty=$(line 1 "printed line 1 of the notes" | cut -d' ' -f2)
    [ "$lx" -le $(( first + 2 )) ] && [ "$lx" -lt "$mx" ] && [ "$mx" -lt "$rx" ] && pass "&l left ($lx), &c centred ($mx), &r right ($rx)" \
        || fail "alignment: left $lx mid $mx right $rx (text at $first)"
    [ "$ly" -lt "$ty" ] && pass "the header is above the text" || fail "header y $ly, text y $ty"
fi
[ -n "$F1" ] && [ -n "$F2" ] && [ -n "$R2" ] && pass "the footer and the page number are on every page" || fail "footer p1 '$F1' p2 '$F2', header p2 '$R2'"
if [ -n "$F1" ]; then
    fy=${F1#* }; lasty=$(awk '$1 == "text" && $2 == 1 && $5 == "printed" { y = $4 } END { print y }' "$T/emf.out")
    [ "$fy" -gt "$lasty" ] && pass "the footer is below the text" || fail "footer y $fy, last line y $lasty"
fi

# --- 4. the light theme: the bar is the system's again ---------------------------------------------
reg 'HKCU\Software\Microsoft\Notepad' /v sgTheme /t REG_DWORD /d 1
"$WINE" notepad.exe >/dev/null 2>&1 &
i=0; while ! "$WINE" 'C:\probe.exe' >/dev/null 2>&1 && [ $i -lt 40 ]; do sleep 0.5; i=$((i + 1)); done
sleep 2
bar
import -window root "$T/light.png"
set -- $(sed -n 's/^bar //p' "$T/bar.out")
if [ $# -eq 4 ]; then
    set -- $(dark_share "$T/light.png" "$1" "$2" "$3" "$4")
    [ "$2" -ge 60 ] && pass "light theme: the menu bar is light ($2% light pixels)" || fail "light theme: menu bar $2% light"
fi
np_quit

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
