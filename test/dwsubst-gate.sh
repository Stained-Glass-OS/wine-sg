#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A name the machine's FontSubstitutes give another font is a DirectWrite
# family (patches/sg/1220). sg-shell maps "Segoe UI" (and Tahoma, Verdana...)
# to the fonts Stained Glass ships; GDI drew "Segoe UI" with its substitute
# while DirectWrite's system collection had no such family, so a program that
# measures and draws with DirectWrite through Skia -- DYMO Connect's labels --
# fell back to the collection's first family: other glyphs, other widths and
# line spacing than the same text on screen (DYMO's "Auto fit" text came out
# small, or cut off at the width measured for the screen's font).
#
# The gate gives "Segoe UI" and a name of its own a substitute (Liberation
# Sans, as Arial's replacement is), then asks DirectWrite, 64- and 32-bit:
# both names are families with the substitute's metrics and widths, listed
# in the collection; GDI-only names ("MS Shell Dlg") and names with a
# character set stay out; an unknown name is still unknown.
#
#   WINE=/opt/wine-sg/bin/wine test/dwsubst-gate.sh
# Mutation: -DSG_MUTANT_DWSUBST in dlls/dwrite/font.c fails it.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" "$MINGW32"; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
fc-list 2>/dev/null | grep -q 'Liberation Sans' || { echo "SKIP: Liberation Sans is not installed"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-dwsubst.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
for m in "$MINGW:64" "$MINGW32:32"; do
    TMPDIR=/var/tmp "${m%:*}" -municode -O2 -o "$T/probe${m#*:}.exe" "$HERE/dwsubst-probe.c" -ldwrite -luuid ||
        { fail "probe did not build"; exit 1; }
done
mkdir -p "$WINEPREFIX"
# no display: not the session's Wayland compositor either (Wine's Wayland
# driver connects to $XDG_RUNTIME_DIR/wayland-0 when WAYLAND_DISPLAY is unset)
mkdir -p -m 700 "$T/xdg"; export XDG_RUNTIME_DIR="$T/xdg"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
K='HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes'
"$WINE" reg add "$K" /v 'Segoe UI' /d 'Liberation Sans' /f >/dev/null 2>&1
"$WINE" reg add "$K" /v 'SG Gate Face' /d 'Liberation Sans' /f >/dev/null 2>&1
"$WINE" reg add "$K" /v 'SG Gate Face CE,238' /d 'Liberation Sans,238' /f >/dev/null 2>&1
"$WINE" reg add "$K" /v 'SG Gate Missing' /d 'No Such Font Xyz' /f >/dev/null 2>&1
"$WINESERVER" -w
for b in 64 32; do
    out=$(cd "$T" && timeout -s KILL 60 "$WINE" "$T/probe$b.exe" "Liberation Sans" "Segoe UI" "SG Gate Face" \
          "MS Shell Dlg" "SG Gate Face CE" "SG Gate Missing" "No Such Font Xyz" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
    lib=$(v 'Liberation Sans'); seg=$(v 'Segoe UI'); own=$(v 'SG Gate Face')
    case "$lib" in exists*) ;; *) echo "SKIP: DirectWrite does not list Liberation Sans"; exit 77 ;; esac
    metrics=$(printf '%s' "$lib" | cut -d'|' -f3-4)
    for n in "Segoe UI:$seg" "SG Gate Face:$own"; do
        r=${n#*:}
        if [ "$(printf '%s' "$r" | cut -d'|' -f1)" = exists ] && [ "$(printf '%s' "$r" | cut -d'|' -f3-4)" = "$metrics" ] &&
           [ "$(printf '%s' "$r" | cut -d'|' -f5)" = listed ]; then
            pass "$b-bit: \"${n%%:*}\" is a family, with its substitute's metrics and widths ($metrics), listed"
        else
            fail "$b-bit: \"${n%%:*}\": '$r' (Liberation Sans: '$lib')"
        fi
    done
    for n in "MS Shell Dlg" "SG Gate Face CE" "SG Gate Missing" "No Such Font Xyz"; do
        [ "$(v "$n")" = none ] && pass "$b-bit: \"$n\" is not a family (GDI-only, a character set's, no font, unknown)" ||
            fail "$b-bit: \"$n\" is a family: '$(v "$n")'"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
