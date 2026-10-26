#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Installers write their fonts into C:\windows\Fonts (patches/sg/6546). Since 6533 the Fonts
# folder holds links to the system's fonts; a font installed over one of those names wrote
# through the link to the system's read-only file: access denied, and LibreOffice's and
# OpenOffice's MSIs stopped (InstallFiles 1603, msiexec 67) at Caladea-Bold.ttf. Such a write
# now replaces the link. A wixl MSI (test/msifont.wxs, embedded cabinet as LibreOffice's)
# installs two fonts into FontsFolder over a link and over a link whose target is gone;
# test/msifont-probe.c: CopyFile and TRUNCATE_EXISTING over links, a link elsewhere still
# refuses (scope), a per-user font install. The system's files are never written.
#   WINE=/opt/wine-sg/bin/wine test/msifont-gate.sh
# Mutant: SG_MUTANT_FONT_LINK_WRITE_THROUGH (dlls/ntdll/unix/file.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb wixl "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
FONT=
for f in "$(dirname "$WINE")/../share/wine/fonts/tahoma.ttf" "$(dirname "$WINE")/fonts/tahoma.ttf" /opt/wine-sg/share/wine/fonts/tahoma.ttf; do
    [ -f "$f" ] && { FONT=$f; break; }
done
[ -n "$FONT" ] || { echo "SKIP: no Wine font to install"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msifont.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -municode -o "$T/probe.exe" "$HERE/msifont-probe.c" -lshell32 -lgdi32 2>/dev/null ||
    TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/msifont-probe.c" -lshell32 -lgdi32 || { fail "probe did not build"; exit 1; }
cp "$FONT" "$T/sgfonttest.ttf"; cp "$FONT" "$T/sgfontgone.ttf"
(cd "$T" && wixl -o font.msi "$HERE/msifont.wxs" >/dev/null 2>&1) || { fail "wixl did not build the package"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
C="$WINEPREFIX/drive_c"
# "the system's fonts": read-only files outside the prefix, linked from the Fonts folder as wineboot links them
mkdir -p "$T/sys" "$C/other"
for n in sgfonttest sgcopy sgtrunc ro; do printf 'system font %s\n' $n > "$T/sys/$n.ttf"; done
chmod 0444 "$T/sys/"*; chmod 0555 "$T/sys"
for n in sgfonttest sgcopy sgtrunc; do ln -sf "$T/sys/$n.ttf" "$C/windows/Fonts/$n.ttf"; done
ln -sf "$T/sys/gone.ttf" "$C/windows/Fonts/sgfontgone.ttf"
ln -sf "$T/sys/ro.ttf" "$C/other/ro.txt"
cp "$T/font.msi" "$T/probe.exe" "$C/"; cp "$FONT" "$C/payload.ttf"
(cd "$C" && timeout -s KILL 300 "$WINE" msiexec /i 'C:\font.msi' /qn /l*v 'C:\font.log' >/dev/null 2>&1); rc=$?
"$WINESERVER" -w
[ $rc = 0 ] && pass "the MSI installs (rc 0)" || { fail "the MSI installs (rc $rc)"; tr -d '\000' < "$C/font.log" 2>/dev/null | grep -a -i "error\|1603" | head -5 | sed 's/^/      /'; }
for n in sgfonttest sgfontgone; do
    f="$C/windows/Fonts/$n.ttf"
    if [ -f "$f" ] && [ ! -L "$f" ] && cmp -s "$f" "$FONT"; then pass "$n.ttf: the package's font replaced the link"
    else fail "$n.ttf: the package's font replaced the link ($(ls -l "$f" 2>&1 | cut -c1-60))"; fi
done
out=$(cd "$C" && timeout -s KILL 60 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "(^| )$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
}
check copy_over_link       "CopyFile over a Fonts link writes the font"
check truncate_over_link   "an overwrite (TRUNCATE_EXISTING) of a Fonts link writes the font"
check other_link_unchanged "a link outside the Fonts folder still writes through (access denied)"
check user_font_copied     "a per-user font is copied into %LOCALAPPDATA%\\Microsoft\\Windows\\Fonts"
check user_font_added      "and AddFontResourceEx loads it"
check done                 "the probe ran to the end"
sys_ok=1
for n in sgfonttest sgcopy sgtrunc ro; do grep -qx "system font $n" "$T/sys/$n.ttf" || sys_ok=0; done
[ -f "$C/windows/Fonts/sgcopy.ttf" ] && [ ! -L "$C/windows/Fonts/sgcopy.ttf" ] && pass "sgcopy.ttf is a file of its own" ||
    fail "sgcopy.ttf is a file of its own"
[ $sys_ok = 1 ] && pass "the system's files are unchanged" || fail "the system's files are unchanged"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
