#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A keyboard layout DLL for the programs that load one (patches/sg/8703).
# GTK 3.24 and 4 (GIMP, Inkscape, darktable, Siril, ART) read Keyboard Layouts\<KLID>\Layout File,
# load that DLL and map keys from its KbdLayerDescriptor tables. Wine had neither, so those programs
# lost Enter, Backspace, the arrows and every shortcut (Siril on qa4, 2026-11-22). The probe does what
# GTK does, as a 64-bit and as a 32-bit program (64-bit pointers in the tables, KBD_LONG_POINTERS),
# on a US keymap: a Layout File, the DLL loads, A gives a/A with CAPLOK at the Shift level, 1 gives
# 1/!, Backspace and Enter are there, Ctrl+C is no character, and scan code 0x1e is VK A.
#   WINE=/opt/wine-sg/bin/wine test/kbdsg-gate.sh      (SG_XDISPLAY=N: Xvfb :N, default 313)
# Mutant: SG_MUTANT_KBDSG_NO_TABLES (dlls/kbdsg/main.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
D=${SG_XDISPLAY:-313}
for t in Xvfb x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -e "/tmp/.X11-unix/X$D" ] && { echo "SKIP: display :$D is taken"; exit 77; }
T=$(mktemp -d /var/tmp/sg-kbdsg.XXXXXX)
XP=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/kbdsg-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/kbdsg-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb ":$D" -screen 0 1024x700x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
sleep 2
export DISPLAY=":$D" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
command -v setxkbmap >/dev/null && setxkbmap -display ":$D" us 2>/dev/null
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
RC=0
for b in 64 32; do
    OUT=$(timeout -s KILL 60 "$WINE" "$T/p$b.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$OUT" | sed "s/^/  [$b] /"
    ok() { if printf '%s\n' "$OUT" | grep -qx "$1"; then echo "PASS  [$b] $2"; else echo "FAIL  [$b] $2"; RC=1; fi; }
    ok 'layout file KBDSG.DLL' "the layout names its DLL (Layout File)"
    ok 'A 0061 0041 attr 1 levels 6' "A gives a, Shift+A gives A, with CAPLOK"
    ok '1 0031 0021' "1 gives 1, Shift+1 gives !"
    ok 'BACK 0008' "Backspace is in the tables"
    ok 'RETURN 000d' "Enter is in the tables"
    ok 'C ctrl f000' "Ctrl+C is not a character (shortcuts stay shortcuts)"
    ok 'vsc1e 0041' "scan code 0x1e is VK A"
done
exit $RC
