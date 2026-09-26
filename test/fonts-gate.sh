#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# fontview.exe, the Fonts folder, per-user fonts and shell: URLs (patches/sg/0183).
#
# Windows has fontview.exe in system32 and opens font files with it; the
# Fonts folder (shell:fonts, %WINDIR%\Fonts) is a view of its own; and since
# Windows 10 1809 a person installs a font without an administrator:
# HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts names the file.
# Checked here: the launchers from a 64- and a 32-bit caller reaching a
# stand-in registered in App Paths with the arguments intact; the font
# associations and their Install verb; a font registered only in HKCU (in a
# folder fontconfig does not know) enumerated by a new process, also in a new
# session, and gone when its value goes; shell:fonts and %WINDIR%\Fonts
# (explorer and ShellExecute) opening the Fonts view; shell:downloads opening
# File Explorer there without looping through ShellExecute.
#
#   WINE=/opt/wine-sg/bin/wine test/fonts-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null && command -v "$MINGW32" >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: needs python3-fonttools"; exit 77; }
SRCFONT=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
[ -f "$SRCFONT" ] || { echo "SKIP: needs fonts-dejavu-core"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fonts-gate.XXXXXX)
# a home of the gate's own: the prefix's user folders and ~/.local/share/fonts are its
mkdir -p "$T/home"
export HOME="$T/home" XDG_DATA_HOME="$T/home/.local/share"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/fonts-probe.c" -lgdi32 &&
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/fonts-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
# the test font: DejaVu Sans under a family name nothing else has
python3 - "$SRCFONT" "$T/sguser.ttf" <<'EOF'
import sys
from fontTools.ttLib import TTFont
f = TTFont(sys.argv[1])
for r in f['name'].names:
    if r.nameID in (1, 4, 16): r.string = 'SG Gate Userfont'
    elif r.nameID == 6: r.string = 'SGGateUserfont'
    elif r.nameID == 3: r.string = 'SGGate:Userfont'
f.save(sys.argv[2])
EOF
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/probe64.exe" "$C/probe64.exe"; cp "$T/probe32.exe" "$C/probe32.exe"; cp "$T/probe64.exe" "$C/standin.exe"
for d in system32 syswow64; do
    [ -f "$C/windows/$d/fontview.exe" ] && pass "$d\\fontview.exe exists" || fail "no $d\\fontview.exe"
done
"$WINE" reg add 'HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\fontview.exe' /ve /d 'C:\standin.exe' /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
q() { "$WINE" reg query "$@" 2>/dev/null | tr -d '\r'; }
ok=1
for e in ttf otf ttc fon; do
    cls=$(q "HKCR\\.$e" /ve | sed -n 's/.*REG_SZ *//p')
    q "HKCR\\$cls\\shell\\open\\command" /ve | grep -qi 'fontview.exe' || ok=0
    q "HKCR\\$cls\\shell\\install\\command" /ve | grep -qi 'fontview.exe" /install' || ok=0
done
[ $ok = 1 ] && pass ".ttf .otf .ttc .fon open in fontview.exe, with an Install verb" || fail "font associations missing"
mkdir -p "$C/fonts dir"; cp "$T/sguser.ttf" "$C/fonts dir/a b.ttf"
"$WINESERVER" -w

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$C"
P() { "$WINE" "\$@" 2>/dev/null | tr -d '\r'; }
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
n=0
mark() { echo "== \$1" >> "$C/standin.log"; }
seen() { n=\$((n + 1)); i=0; while [ "\$(grep -c '^cmdline=' "$C/standin.log" 2>/dev/null)" -lt \$n ] && [ \$i -lt 20 ]; do sleep 0.5; i=\$((i + 1)); done; }
mark cp64;    P probe64.exe run 'fontview.exe "C:\\fonts dir\\a b.ttf"' >/dev/null; seen
mark cp32;    P probe32.exe run 'fontview /p "C:\\fonts dir\\a b.ttf"' >/dev/null; seen
mark open;    P probe64.exe open 'C:\\fonts dir\\a b.ttf' >/dev/null; seen
mark install; P probe64.exe open 'C:\\fonts dir\\a b.ttf' install >/dev/null; seen
# (a File Explorer window instead would stay open: bounded)
mark shellfonts; timeout -s KILL 20 "$WINE" explorer shell:fonts >/dev/null 2>&1; seen
mark windir;  timeout -s KILL 20 "$WINE" explorer 'C:\\windows\\Fonts' >/dev/null 2>&1; seen
mark run;     P probe64.exe open 'shell:fonts' >/dev/null; seen
mark end
P probe64.exe open 'shell:downloads' > "$T/dl.out"
sleep 4
P probe64.exe title Downloads >> "$T/dl.out"
P probe64.exe count explorer.exe >> "$T/dl.out"
sleep 3
P probe64.exe count explorer.exe >> "$T/dl.out"
"$WINESERVER" -k
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x768x24' "$T/session.sh"
"$WINESERVER" -w

L="$C/standin.log"
after() { sed -n "/^== $1\$/,/^== /p" "$L" 2>/dev/null | sed -n 's/^cmdline=//p' | head -1; }
case "$(after cp64)" in *'"C:\fonts dir\a b.ttf"'*) pass "a 64-bit CreateProcess(\"fontview.exe <file>\") reaches App Paths' program with the file";; *) fail "64-bit fontview.exe: '$(after cp64)'";; esac
case "$(after cp32)" in *'/p "C:\fonts dir\a b.ttf"'*) pass "a 32-bit caller's fontview /p <file> too (syswow64)";; *) fail "32-bit fontview: '$(after cp32)'";; esac
case "$(after open)" in *'C:\fonts dir\a b.ttf'*) pass "ShellExecute of a .ttf opens it in fontview";; *) fail "open .ttf: '$(after open)'";; esac
case "$(after install)" in *'/install'*'C:\fonts dir\a b.ttf'*) pass "the .ttf's Install verb runs fontview /install <file>";; *) fail "install verb: '$(after install)'";; esac
case "$(after shellfonts)" in *'/folder'*) pass "explorer shell:fonts opens the Fonts view (fontview /folder)";; *) fail "explorer shell:fonts: '$(after shellfonts)'";; esac
case "$(after windir)" in *'/folder'*) pass "explorer %WINDIR%\\Fonts opens the Fonts view";; *) fail "explorer C:\\windows\\Fonts: '$(after windir)'";; esac
case "$(after run)" in *'/folder'*) pass "ShellExecute(\"shell:fonts\") -- the Run box -- opens the Fonts view";; *) fail "ShellExecute shell:fonts: '$(after run)'";; esac
grep -q '^found=1' "$T/dl.out" && pass "ShellExecute(\"shell:downloads\") opens File Explorer on Downloads" || fail "shell:downloads: $(tr '\n' ' ' < "$T/dl.out")"
c1=$(sed -n 's/^count=//p' "$T/dl.out" | sed -n 1p); c2=$(sed -n 's/^count=//p' "$T/dl.out" | sed -n 2p)
[ "${c1:-99}" -le 3 ] && [ "${c1:-99}" = "${c2:-0}" ] && pass "and explorer does not send it round again ($c1 explorer processes, steady)" \
    || fail "explorer processes: $c1 then $c2 (a ShellExecute loop?)"

# per-user fonts: HKCU only, in a folder fontconfig does not scan
P() { "$WINE" "$@" 2>/dev/null | tr -d '\r'; }
has() { P "$C/probe64.exe" has 'SG Gate Userfont' | sed -n 's/^has=//p'; }
[ "$(has)" = 0 ] && pass "the test family is not installed to begin with" || fail "the test family is there before it is installed"
U=$(P cmd /c 'echo %LOCALAPPDATA%')
UD="$(P winepath -u "$U")/Microsoft/Windows/Fonts"
mkdir -p "$UD"; cp "$T/sguser.ttf" "$UD/sguser.ttf"
"$WINE" reg add 'HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts' /v 'SG Gate Userfont (TrueType)' /d "$U\\Microsoft\\Windows\\Fonts\\sguser.ttf" /f >/dev/null 2>&1
[ "$(has)" = 1 ] && pass "a font named only in HKCU\\...\\Fonts is enumerated by a new process" || fail "per-user font not loaded (HKCU Fonts ignored)"
"$WINESERVER" -k; "$WINESERVER" -w
[ "$(has)" = 1 ] && pass "and in a new session (not from a cache)" || fail "per-user font gone in a new session"
"$WINE" reg delete 'HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts' /v 'SG Gate Userfont (TrueType)' /f >/dev/null 2>&1
"$WINESERVER" -k; "$WINESERVER" -w
[ "$(has)" = 0 ] && pass "its value deleted, the font is gone" || fail "font still enumerated after its value was deleted"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
