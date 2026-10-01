#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A Linux program's window on the taskbar shows the program's own icon
# (patches/sg/0612). Start's Linux apps (sg-shell's sg-linuxapp) put each
# app's .ico under its window class's names in %LOCALAPPDATA%\Stained Glass\
# Linux app icons; the button of a window of that class takes it. Here a
# stand-in sg-lockctl lists one window of class gate-calc, and a red
# gate-calc.ico is there: its button must show red.
#
#   WINE=/opt/wine-sg/bin/wine test/linuxicon-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v cc >/dev/null || { echo "SKIP: needs $MINGW and cc"; exit 77; }
command -v xvfb-run >/dev/null && command -v import >/dev/null || { echo "SKIP: needs xvfb-run and ImageMagick"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-linuxicon.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
cc -O2 -o "$T/fake-lockctl" "$HERE/fake-lockctl.c" || { fail "stand-in did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
ICONS="$WINEPREFIX/drive_c/users/$(id -un)/AppData/Local/Stained Glass/Linux app icons"
mkdir -p "$ICONS"
# a 32 x 32 icon, solid red (a 32-bit DIB and its mask)
python3 - "$ICONS/gate-calc.ico" <<'EOF'
import struct, sys
n = 32
xor = b"\x00\x00\xff\xff" * (n * n)
mask = b"\x00" * (4 * n)
dib = struct.pack("<IiiHHIIiiII", 40, n, 2 * n, 1, 32, 0, len(xor) + len(mask), 0, 0, 0, 0) + xor + mask
open(sys.argv[1], "wb").write(struct.pack("<HHH", 0, 1, 1) + struct.pack("<BBBBHHII", n, n, 0, 0, 1, 32, len(dib), 22) + dib)
EOF
TAB=$(printf '\t')
printf '4242 shown - gate-calc%sGate Calculator\nEND\n' "$TAB" > "$T/list"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent SG_LOCKCTL="$T/fake-lockctl" SG_FAKE_DIR="$T" XDG_RUNTIME_DIR="$T/run"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 5
"$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' > "$T/state.out"
import -window root "$T/bar.png"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
B=$(sed -n 's/^button=//p' "$T/state.out" | head -1)
[ -n "$B" ] || { fail "no button for the Linux window: $(cat "$T/state.out")"; exit 1; }
red=$(python3 - "$T/bar.png" "$B" <<'EOF'
import subprocess, sys
l, t, r, b = map(int, sys.argv[2].split(","))
out = subprocess.run(["convert", sys.argv[1], "-crop", "%dx%d+%d+%d" % (r - l, b - t, l, t), "txt:-"],
                     capture_output=True, text=True).stdout
n = 0
for line in out.splitlines()[1:]:
    try:
        R, G, B = (float(v.strip().rstrip("%")) for v in line.split("(")[1].split(")")[0].split(",")[:3])
    except Exception:
        continue
    if R > 200 and G < 60 and B < 60:
        n += 1
print(n)
EOF
)
[ "${red:-0}" -ge 40 ] && pass "the button shows the app's icon ($red red pixels)" ||
    fail "the button has no icon of the app ($red red pixels)"
exit $RC
