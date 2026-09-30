#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A pinned program's taskbar button shows the program's icon (patches/sg/0602).
# The shell gives a program's shortcut the blank page with an arrow, and File
# Explorer (explorer.exe carries no icon) and Edge pinned showed that (David
# 2026-09-30). File Explorer pinned must show the folder: yellow on the button.
#
#   WINE=/opt/wine-sg/bin/wine test/pinicon-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-pinicon.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
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
"$WINE" 'C:\mklnk.exe' 'C:\users\'"$(id -un)"'\AppData\Roaming\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\File Explorer.lnk' 'C:\windows\explorer.exe'
"$WINESERVER" -w
ls "$PINS" | grep -q 'File Explorer.lnk' || { fail "the pin was not made"; exit 1; }

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! "$WINE" taskbar-probe.exe state 2>/dev/null | grep -q '^pin='; do sleep 1; i=\$((i + 1)); [ \$i -gt 60 ] && break; done
sleep 3
"$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' > "$T/state.out"
import -window root "$T/bar.png"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
pin=$(sed -n 's/^pin=//p' "$T/state.out" | head -1)
[ -n "$pin" ] || { fail "no pin button on the bar: $(cat "$T/state.out")"; exit 1; }
# yellow: red and green high, blue low -- the folder; the blank page is white and grey
yellow=$(python3 - "$T/bar.png" "$pin" <<'EOF'
import subprocess, sys
l, t, r, b = map(int, sys.argv[2].split(","))
out = subprocess.run(["convert", sys.argv[1], "-crop", "%dx%d+%d+%d" % (r - l, b - t, l, t), "txt:-"],
                     capture_output=True, text=True).stdout
n = 0
for line in out.splitlines()[1:]:
    try:
        rgb = line.split("(")[1].split(")")[0].split(",")[:3]
        R, G, B = (float(v.strip().rstrip("%")) for v in rgb)
    except Exception:
        continue
    if R > 180 and G > 140 and B < 110:
        n += 1
print(n)
EOF
)
[ "${yellow:-0}" -ge 20 ] && pass "File Explorer pinned shows the folder icon ($yellow yellow pixels on its button)" \
    || fail "File Explorer's pin shows no folder (yellow pixels: ${yellow:-0}; button $pin)"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
