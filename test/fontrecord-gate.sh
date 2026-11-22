#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A host font's entry in the Fonts key comes back when something took it away (patches/sg/8417).
# win32u writes each host font (fontconfig's) into HKLM\...\Fonts and remembers that it did in
# HKCU\Software\Wine\Fonts\External Fonts; on the next start a remembered font was taken as still
# listed. An installer that ships the same font removes "Liberation Sans (TrueType)" when it is
# uninstalled (LibreOffice's MSI); the entry stayed gone for good, and with it 7711's arial.ttf
# link to Liberation Sans, so Java programs (Scilab 2027, qa5) drew every letter as a box.
# Now a remembered font whose entry is missing is written again; an entry an installer changed
# to its own file is left alone.
#   WINE=/opt/wine-sg/bin/wine test/fontrecord-gate.sh
# Mutant: SG_MUTANT_TRUST_EXTERNAL_RECORD (dlls/win32u/font.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" fc-list; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
fc-list : family | grep -qx 'Liberation Sans' || { echo "SKIP: no Liberation fonts on the host"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fontrecord.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fontrecord-probe.c" -lgdi32 || { fail "probe did not build"; exit 1; }
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
F="$WINEPREFIX/drive_c/windows/Fonts"
K='HKLM\Software\Microsoft\Windows NT\CurrentVersion\Fonts'
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
probe() { timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r'; "$WINESERVER" -w; }
val() { timeout -s KILL 60 "$WINE" reg query "$K" /v "$1" 2>/dev/null | tr -d '\r' | sed -n "s/^ *$1 *REG_SZ *//p"; }
# take the entry away inside one wineserver run whose fonts have started (a new run's first font
# start -- services, explorer -- would put it back at once), then end that run; GONE: it was gone
del() {
    "$WINESERVER" -p
    timeout -s KILL 120 "$WINE" 'C:\probe.exe' >/dev/null 2>&1
    timeout -s KILL 60 "$WINE" reg delete "$K" /v "$1" /f >/dev/null 2>&1
    GONE=$([ -z "$(val "$1")" ] && echo 1)
    "$WINESERVER" -k; "$WINESERVER" -w
}

N='Liberation Sans (TrueType)'
before=$(val "$N")
case "$before" in [Zz]:*LiberationSans-Regular.ttf) pass "a fresh prefix lists $N ($before)";;
    *) fail "a fresh prefix: $N = '$before'"; exit 1;; esac
[ -L "$F/arial.ttf" ] && pass "and links arial.ttf to it (7711)" || fail "no arial.ttf link in a fresh prefix"

# an uninstaller took the entry away; the next program's font start puts it back
del "$N"
[ "$GONE" = 1 ] && pass "the entry was taken away" || fail "reg delete left it"
probe >/dev/null
after=$(val "$N")
[ "$after" = "$before" ] && pass "the next program's font start writes it again ($after)" \
    || fail "after a program's font start: $N = '$after' (was '$before')"

# and wineboot -u (an upgrade) links arial.ttf again when both had gone
del "$N"; rm -f "$F/arial.ttf"
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1; "$WINESERVER" -w
case "$(readlink "$F/arial.ttf" 2>/dev/null)" in *LiberationSans-Regular.ttf) pass "an update links arial.ttf to Liberation Sans again";;
    *) fail "after an update: arial.ttf -> '$(readlink "$F/arial.ttf" 2>/dev/null)', entry '$(val "$N")'";; esac

# an entry an installer pointed at its own file stays the installer's
timeout -s KILL 60 "$WINE" reg add "$K" /v "$N" /d 'LiberationSans-Regular.ttf' /f >/dev/null 2>&1; "$WINESERVER" -w
probe >/dev/null
[ "$(val "$N")" = 'LiberationSans-Regular.ttf' ] && pass "an installer's own entry for it is left alone" \
    || fail "an installer's own entry became '$(val "$N")'"

echo
if [ "$RC" = 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
