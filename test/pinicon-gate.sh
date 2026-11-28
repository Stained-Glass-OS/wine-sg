#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A pinned program's taskbar button shows the program's icon (patches/sg/0602).
# The shell gives a program's shortcut the blank page with an arrow, and File
# Explorer (explorer.exe carries no icon) and Edge pinned showed that (David
# 2026-09-30). File Explorer pinned must show File Explorer's own icon -- since
# wine-sg 6269 explorer.exe carries it (a folder with a band, in the accent
# colour: BlueViolet in a new prefix), not the shortcut's white page.
#
#   WINE=/opt/wine-sg/bin/wine test/pinicon-gate.sh
#   SG_PINICON_MUTANT=PAGE: the pin's shortcut takes shell32's blank page as its
#   icon (what the button showed before 0602): must FAIL.
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
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-pinicon.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; sg_kill_prefix "$WINEPREFIX"; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
cat > "$T/mklnk.c" <<'EOF'
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
int wmain(int argc, WCHAR **argv)
{
    IShellLinkW *l; IPersistFile *f;
    if (argc < 3) return 2;
    CoInitialize(NULL);
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&l))) return 1;
    IShellLinkW_SetPath(l, argv[2]);
    if (argc > 3) IShellLinkW_SetIconLocation(l, argv[3], 0);
    IShellLinkW_QueryInterface(l, &IID_IPersistFile, (void **)&f);
    return FAILED(IPersistFile_Save(f, argv[1], TRUE));
}
EOF
"$MINGW" -O2 -municode -o "$T/mklnk.exe" "$T/mklnk.c" -lole32 -luuid || { fail "mklnk did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$T/mklnk.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
# Horizon: the look pinned programs show in (wine-sg 0600)
"$WINE" reg add 'HKCU\Software\Stained Glass' /v 'Taskbar Style' /t REG_DWORD /d 1 /f >/dev/null 2>&1
PINS="$WINEPREFIX/drive_c/users/$(id -un)/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar"
mkdir -p "$PINS"
"$WINE" 'C:\mklnk.exe' 'C:\users\'"$(id -un)"'\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\File Explorer.lnk' 'C:\windows\explorer.exe' \
    ${SG_PINICON_MUTANT:+'C:\windows\system32\shell32.dll'}
"$WINESERVER" -w
ls "$PINS" | grep -q 'File Explorer.lnk' || { fail "the pin was not made"; exit 1; }

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
# the shell's desktop first: a probe started before it made the desktop
# itself, and the shell's explorer then never put up its taskbar
i=0; while ! xwininfo -root -tree 2>/dev/null | grep -q '"shell'; do sleep 0.5; i=\$((i + 1)); [ \$i -gt 120 ] && break; done
i=0; while ! "$WINE" taskbar-probe.exe state 2>/dev/null | grep -q '^pin='; do sleep 1; i=\$((i + 1)); [ \$i -gt 60 ] && break; done
sleep 3
"$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' > "$T/state.out"
import -window root "$T/bar.png"
xwininfo -root -tree > "$T/tree.out" 2>&1
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
pin=$(sed -n 's/^pin=//p' "$T/state.out" | head -1)
[ -n "$pin" ] || { fail "no pin button on the bar: $(cat "$T/state.out")"; exit 1; }
# File Explorer's icon: mostly the accent's hue (BlueViolet, 271 degrees, in a
# new prefix); the blank page is white and light grey
counts=$(python3 - "$T/bar.png" "$pin" <<'EOF'
import colorsys, subprocess, sys
l, t, r, b = map(int, sys.argv[2].split(","))
out = subprocess.run(["convert", sys.argv[1], "-crop", "%dx%d+%d+%d" % (r - l, b - t, l, t), "-depth", "8", "txt:-"],
                     capture_output=True, text=True).stdout
accent = page = 0
for line in out.splitlines()[1:]:
    try:
        R, G, B = (int(v) for v in line.split("(")[1].split(")")[0].split(",")[:3])
    except Exception:
        continue
    h, s, v = colorsys.rgb_to_hsv(R / 255, G / 255, B / 255)
    if 250 <= h * 360 <= 295 and s >= 0.35 and v >= 0.35:
        accent += 1
    elif s < 0.12 and v > 0.75:
        page += 1
print(accent, page)
EOF
)
accent=${counts% *} page=${counts#* }
[ "${accent:-0}" -ge 40 ] && [ "${accent:-0}" -gt "${page:-0}" ] \
    && pass "File Explorer pinned shows File Explorer's icon ($accent accent-coloured pixels, $page white on its button)" \
    || fail "File Explorer's pin does not show File Explorer's icon (accent-coloured pixels: ${accent:-0}, white: ${page:-0}; button $pin)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
